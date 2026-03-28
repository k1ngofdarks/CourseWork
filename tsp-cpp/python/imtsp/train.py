import comet_ml
import torch
import torch.nn as nn
from model import UpperLevelIMTSP, TSPSolver
from tqdm.notebook import tqdm
import json


def generate_data(batch_size, n, n_depots, scale, device):
    """
    Generate graphs and depots for training
    :param batch_size: int
    :param n: int
    :param n_depots: int
    :param scale: float, all coordinates samples from [0; scale] int
    :param device
    :return: torch.Tensor generated coords of vertices (batch_size, n, 2), torch.Tensor generated depots (batch_size, n_depots)
    """
    coords = torch.rand((batch_size, n, 2), device=device) * scale
    weights = torch.ones(n, device=device).expand(batch_size, -1)
    depots = torch.multinomial(weights, num_samples=n_depots, replacement=False)
    return coords, depots


def train_batch(model: UpperLevelIMTSP, solver: TSPSolver, allocation_optimizer: torch.optim.Optimizer,
                surrogate_optimizer: torch.optim.Optimizer,
                allocation_scheduler: torch.optim.lr_scheduler.LRScheduler,
                surrogate_scheduler: torch.optim.lr_scheduler.LRScheduler, coords, depots):
    """
    :param model: UpperLevelIMTSP instance
    :param solver: TSPSolver instance
    :param allocation_optimizer: optimizer for updating weights of allocation network
    :param surrogate_optimizer: optimizer for updating weights of surrogate network
    :param allocation_scheduler: scheduler for updating allocation lr
    :param surrogate_scheduler: scheduler for updating surrogate lr
    :param coords: torch.Tensor coords of vertices -- (batch_size, n, 2)
    :param depots: torch.Tensor indices of depots -- (batch_size, n_depots)
    :return allocation_loss, surrogate_loss, mean of found max length of routes
    """
    model.train()

    batch_size, n, _ = coords.shape
    n_depots = depots.shape[1]

    customer_log_probs, predicted_lengths, agent_indices = model(coords, depots)  # (batch_size, n), (batch_size,), (batch_size, n)

    # dist = torch.distributions.Categorical(logits=logits.transpose(1, 2).reshape(batch_size * n, n_depots))
    # agent_indices = dist.sample()
    # log_probs = dist.log_prob(agent_indices).reshape(batch_size, n)  # (batch_size, n)
    # agent_indices = agent_indices.reshape(batch_size, n)

    with torch.no_grad():
        found_lengths = solver.solve(agent_indices, coords)  # (batch_size,)

    allocation_scheduler.step(found_lengths.sum().item())
    surrogate_scheduler.step(found_lengths.sum().item())

    baseline = found_lengths.mean().item()

    # allocation_loss = ((found_lengths - 2) * customer_log_probs.sum(dim=1)).sum() - ((predicted_lengths.detach() - 2) * customer_log_probs.sum(dim=1)).sum() + (predicted_lengths - 2).sum()
    allocation_loss = ((found_lengths - baseline - predicted_lengths.detach()) * customer_log_probs.sum(dim=1)).sum() \
                      + predicted_lengths.sum()
    # allocation_loss = ((found_lengths - baseline) * customer_log_probs.sum(dim=1)).sum() - ((predicted_lengths.detach() - baseline) * customer_log_probs.sum(dim=1)).sum() + (predicted_lengths - baseline).sum()
    # allocation_loss = ((found_lengths - predicted_lengths).detach() * log_probs.sum(dim=1) + predicted_lengths).sum()
    alloc_params = list(model.gnn.parameters()) + list(model.alloc_network.parameters())
    alloc_grad = torch.autograd.grad(allocation_loss, alloc_params, create_graph=True)

    grad_sq_sum = sum((param ** 2).sum() for param in alloc_grad)
    n_params = sum(param.numel() for param in alloc_grad)
    surrogate_loss = grad_sq_sum / n_params

    surrogate_grad = torch.autograd.grad(surrogate_loss, model.surrogate_network.parameters(), allow_unused=True)

    surrogate_optimizer.zero_grad()
    for param, grad in zip(model.surrogate_network.parameters(), surrogate_grad):
        if grad is not None:
            param.grad = grad
    surrogate_optimizer.step()

    allocation_optimizer.zero_grad()
    for param, grad in zip(alloc_params, alloc_grad):
        if grad is not None:
            param.grad = grad.detach()
    allocation_optimizer.step()

    return allocation_loss.item(), surrogate_loss.item(), found_lengths.mean().item(), (
            found_lengths - predicted_lengths).mean().item()


@torch.no_grad()
def validation(model: UpperLevelIMTSP, solver: TSPSolver, valid_set_path: str, device, lengths=(500, 1000), exp=None,
               epoch=0):
    model.eval()
    with open(valid_set_path, "r") as f:
        valid_data = json.load(f)
    assert len(lengths) == len(valid_data["depots"])
    result = []
    for i in tqdm(range(len(lengths)), desc="Validation"):
        coords = torch.tensor(valid_data["coords"][i])
        depots = torch.tensor(valid_data["depots"][i], dtype=torch.int32)

        batch_size = len(depots)
        test_n = lengths[i]

        full_graph_matrix = torch.argwhere(torch.ones((test_n, test_n)) - torch.eye(test_n)).to(device)
        range_gnn = torch.arange(0, test_n * batch_size, test_n, device=device)
        edge_index = (range_gnn[:, None, None] + full_graph_matrix[None]).reshape(test_n * (test_n - 1) * batch_size, 2).T
        depots_gnn = torch.zeros((batch_size, test_n), device=device)
        depots_gnn[torch.arange(batch_size, device=device)[:, None], depots] = True
        batch_gnn = torch.arange(batch_size, device=device).repeat_interleave(test_n)

        vertices_embeds, graph_context = model.gnn(coords.reshape(test_n * batch_size, 2), edge_index, depots_gnn.reshape(-1), batch_gnn)
        vertices_embeds = vertices_embeds.reshape(batch_size, test_n, -1)
        logits = model.alloc_network(vertices_embeds, graph_context, depots)

        agent_indices = logits.argmax(dim=1)  # (batch_size, test_n)

        found_lengths = solver.solve(agent_indices, coords)
        result.append(found_lengths.mean().item())

    if exp is not None:
        exp.log_metrics({f"mean_length_val_{length}": metric for length, metric in zip(lengths, result)}, epoch=epoch)

    return result


def train(batch_size, n, n_depots, scale, n_epochs, n_iter_per_epoch, model: UpperLevelIMTSP, solver: TSPSolver,
          allocation_optimizer: torch.optim.Optimizer, surrogate_optimizer: torch.optim.Optimizer,
          allocation_scheduler: torch.optim.lr_scheduler.LRScheduler,
          surrogate_scheduler: torch.optim.lr_scheduler.LRScheduler, device, valid_lengths=(500, 1000),
          valid_set_path: str = None, prefix: str = "imtsp"):
    alloc_loss_log = []
    surr_loss_log = []
    mean_length_log = []
    mean_length_diff = []

    exp = comet_ml.start(project_name='coursework_imtsp', mode='create')
    checkpoint = {
        'epoch': 0,
        'model_state_dict': model.state_dict(),
        'alloc_optimizer_state': allocation_optimizer.state_dict(),
        'surr_optimizer_state': surrogate_optimizer.state_dict(),
        'alloc_scheduler_state': allocation_scheduler.state_dict(),
        'surr_scheduler_state': surrogate_scheduler.state_dict(),
    }
    torch.save(checkpoint, f"{prefix}_0_epoch.pth")
    exp.log_asset(f"{prefix}_0_epoch.pth", file_name=f"{prefix}_0_epoch.pth")

    for num_epoch in tqdm(range(1, n_epochs + 1), desc='Training'):
        alloc_loss_sum = 0.0
        surr_loss_sum = 0.0
        mean_length_sum = 0.0
        mean_length_diff_sum = 0.0
        for num_iter in tqdm(range(n_iter_per_epoch), desc=f'Training on {num_epoch} epoch'):
            coords, depots = generate_data(batch_size, n, n_depots, scale, device)
            allocation_loss, surrogate_loss, mean_length, mean_length_diff = \
                train_batch(model, solver, allocation_optimizer, surrogate_optimizer, allocation_scheduler,
                            surrogate_scheduler, coords, depots)
            alloc_loss_sum += allocation_loss
            surr_loss_sum += surrogate_loss
            mean_length_sum += mean_length
            mean_length_diff_sum += mean_length_diff
            print(f"alloc_loss={allocation_loss :.4f}, surr_loss={surrogate_loss :.4f},"
                  f" mean_length={mean_length :.4f}, mean_length_diff={mean_length_diff :.4f},"
                  f" alloc_lr={allocation_scheduler.get_last_lr()[0] :.7f},"
                  f" surr_lr={surrogate_scheduler.get_last_lr()[0] :.7f}")
            exp.log_metrics({
                "allocation_loss": allocation_loss,
                "surrogate_loss": surrogate_loss,
                "mean_length": mean_length,
                "mean_length_diff": mean_length_diff,
                "allocation_lr": allocation_scheduler.get_last_lr()[0],
                "surrogate_lr": surrogate_scheduler.get_last_lr()[0]
            }, step=(num_epoch - 1) * n_iter_per_epoch + num_iter + 1)
        alloc_loss_sum /= n_iter_per_epoch
        surr_loss_sum /= n_iter_per_epoch
        mean_length_sum /= n_iter_per_epoch
        alloc_loss_log.append(alloc_loss_sum)
        surr_loss_log.append(surr_loss_sum)
        mean_length_log.append(mean_length_sum)
        checkpoint = {
            'epoch': num_epoch,
            'model_state_dict': model.state_dict(),
            'alloc_optimizer_state': allocation_optimizer.state_dict(),
            'surr_optimizer_state': surrogate_optimizer.state_dict(),
            'alloc_scheduler_state': allocation_scheduler.state_dict(),
            'surr_scheduler_state': surrogate_scheduler.state_dict(),
        }
        torch.save(checkpoint, f"{prefix}_{num_epoch}_epoch.pth")
        exp.log_asset(f"{prefix}_{num_epoch}_epoch.pth", file_name=f"{prefix}_{num_epoch}_epoch.pth")
        print(f"epoch={num_epoch}: alloc_loss={alloc_loss_sum :.4f}, surr_loss={surr_loss_sum :.4f},"
              f" mean_length={mean_length_sum :.4f}, mean_length_diff={mean_length_diff :.7f},"
              f" alloc_lr={allocation_scheduler.get_last_lr()[0] :.7f},"
              f" surr_lr={surrogate_scheduler.get_last_lr()[0] :.4f}")
        if valid_set_path is not None:
            valid_metrics = validation(model, solver, valid_set_path, device, valid_lengths, exp, num_epoch)
            for length, metric in zip(valid_lengths, valid_metrics):
                print(f"n_nodes={length}: mean_length_val={metric}")
        with open("test_logs.txt", "a") as f:
            print(f"epoch={num_epoch}: alloc_loss={alloc_loss_sum :.4f}, surr_loss={surr_loss_sum :.4f},"
                  f" mean_length={mean_length_sum :.4f}, mean_length_diff={mean_length_diff :.4f},"
                  f" alloc_lr={allocation_scheduler.get_last_lr()[0] :.7f},"
                  f" surr_lr={surrogate_scheduler.get_last_lr()[0] :.7f}", file=f)

    exp.end()
    return alloc_loss_log, surr_loss_log, mean_length_log
