import torch
import torch.nn as nn
import math
from torch_geometric.nn import GINConv, global_mean_pool
import lkh
from concurrent.futures import ThreadPoolExecutor, as_completed


class GraphEmbeddings(nn.Module):
    def __init__(self, in_size=3, embed_size=64, n_message_layers=3):
        super().__init__()

        self.block1 = nn.Sequential(nn.Linear(in_size, embed_size), nn.BatchNorm1d(embed_size), nn.ReLU(), nn.Linear(embed_size, embed_size))
        self.block2 = nn.Sequential(nn.Linear(embed_size, embed_size), nn.BatchNorm1d(embed_size))

        self.messages_layers = nn.ModuleList()
        self.batch_norms = nn.ModuleList()

        for _ in range(n_message_layers):
            mlp_layers = nn.Sequential(nn.Linear(embed_size, embed_size),
                                       # nn.BatchNorm1d(embed_size),
                                       nn.ReLU(),
                                       nn.Linear(embed_size, embed_size),
                                       # nn.ReLU()
                                       )
            conv = GINConv(mlp_layers, aggr="mean")
            self.messages_layers.append(conv)
            self.batch_norms.append(nn.BatchNorm1d(embed_size))

    def forward(self, coords, edge_index, depots, batch):
        """
        :param coords: torch.Tensor (N_total, 2) -- coords of vertices
        :param edge_index: torch.LongTensor (2, E_total) -- all edges in batch graphs
        :param depots: torch.BoolTensor (N_total) -- depots[i] == 1 iff v_i is depot
        :param batch: torch.LongTensor (N_total) -- batch[i] = j iff v_i belongs to j-th graph
        :return: vertices_embeddings (N_total, embed_size), graph_context (batch_size, embed_size)
        """
        embeddings = torch.cat([coords, depots[:, None].float()], dim=-1)
        embeddings = self.block1(embeddings)

        vertices_embeds = embeddings.clone()

        for conv, bn in zip(self.messages_layers, self.batch_norms):
            embeddings = conv(embeddings, edge_index)
            embeddings = bn(embeddings)
            embeddings = torch.relu(embeddings)
            vertices_embeds += embeddings

        graph_context = global_mean_pool(vertices_embeds, batch)
        graph_context = self.block2(graph_context)

        return vertices_embeds, graph_context


class AgentEmbeddings(nn.Module):
    def __init__(self, embed_size=64, output_size=16, dropout=0.3):
        super().__init__()

        self.Wk = nn.Linear(embed_size, embed_size)
        self.Wq = nn.Linear(2 * embed_size, embed_size)
        self.Wv = nn.Linear(embed_size, embed_size)
        self.Wo = nn.Linear(embed_size, output_size)

        self.dropout = nn.Dropout(dropout)

    def forward(self, vertices_embeds, context_vector, depots_id):
        """
        :param vertices_embeds: torch.Tensor (batch_size, n, embed_size)
        :param context_vector: torch.Tensor context vectors of graphs -- (batch_size, embed_size)
        :param depots_id: torch.LongTensor indices of depots -- (batch_size, n_depots)
        :return: torch.Tensor embeds of agents -- (batch_size, n_depots, output_size)
        """
        batch_size, n_depots = depots_id.shape
        fc = vertices_embeds[torch.arange(batch_size)[:, None], depots_id]  # (batch_size, n_depots, embed_size)
        fc = torch.cat([context_vector[:, None, :].expand(-1, n_depots, -1), fc],
                       dim=-1)  # (batch_size, n_depots, 2*embed_size)

        Q = self.Wq(fc)  # (batch_size, n_depots, embed_size)
        K = self.Wk(vertices_embeds)
        V = self.Wv(vertices_embeds)  # K, V - (batch_size, n, embed_size)
        U = torch.bmm(Q, K.transpose(1, 2)) / math.sqrt(Q.shape[-1])  # (batch_size, n_depots, n)
        W = torch.softmax(U, dim=-1)
        H = torch.bmm(W, V)  # (batch_size, n_depots, embed_size)
        attention_result = self.Wo(H)
        # return self.dropout(attention_result)
        return attention_result


class AllocationNetwork(nn.Module):
    def __init__(self, graph_embed_size=64, agent_embed_size=16, hidden_size=128, clip_constant=10, dropout=0.3):
        super().__init__()

        self.agent_embeddings = AgentEmbeddings(graph_embed_size, agent_embed_size, dropout)

        self.Wk = nn.Linear(graph_embed_size, hidden_size)
        self.Wq = nn.Linear(agent_embed_size, hidden_size)

        self.clip_constant = clip_constant

    def forward(self, vertices_embeds, context_vector, depots_id):
        """
        :param vertices_embeds: torch.Tensor (batch_size, n, graph_embed_size)
        :param context_vector: torch.Tensor context vectors of graphs -- (batch_size, graph_embed_size)
        :param depots_id: torch.LongTensor indices of depots -- (batch_size, n_depots)
        :return: torch.Tensor logits -- (batch_size, n_depots, n)
        """
        agent_embeds = self.agent_embeddings(vertices_embeds, context_vector,
                                             depots_id)  # (batch_size, n_depots, agent_embed_size)

        K = self.Wk(vertices_embeds)  # (batch_size, n, hidden_size)
        Q = self.Wq(agent_embeds)  # (batch_size, n_depots, hidden_size)
        U = torch.bmm(Q, K.transpose(1, 2)) / math.sqrt(K.shape[-1])  # (batch_size, n_depots, n)
        logits = torch.tanh(U) * self.clip_constant

        batch_size, n_depots, n = U.shape
        mask = torch.zeros(batch_size, n, device=vertices_embeds.device, dtype=torch.bool)
        mask[torch.arange(batch_size)[:, None], depots_id] = True
        logits = logits.masked_fill(mask[:, None], -torch.inf)
        logits[torch.arange(batch_size)[:, None], torch.arange(n_depots)[None], depots_id] = 0

        return logits


# class TSPSolver:
#     def __init__(self, edge_weight_type="EUC_2D", solver_path="LKH"):
#
#         self.edge_weight_type = edge_weight_type
#         self.const_params = {"name": "TSP_Task", "type": "TSP", "edge_weight_type": edge_weight_type}
#         self.solver_path = solver_path
#
#     def calculate_distance_matrix_(self, coords):
#         """
#         :param coords: torch.Tensor -- (batch_size, n, 2)
#         :return: torch.Tensor distance matrix -- (batch_size, n, n)
#         """
#         if self.edge_weight_type == "EUC_2D":
#             return torch.cdist(coords, coords)
#         elif self.edge_weight_type != "GEO_2D":
#             raise NotImplemented
#
#         R = 6371.0
#         coords_rad = coords * (math.pi / 180.0)
#         lat = coords_rad[:, :, 0]
#         lon = coords_rad[:, :, 1]
#
#         lat_i = lat[:, :, None]
#         lat_j = lat[:, None]
#         dlat = lat_j - lat_i
#         dlon = lon[:, None] - lon[:, :, None]
#
#         a = torch.sin(dlat / 2) ** 2 + torch.cos(lat_i) * torch.cos(lat_j) * torch.sin(dlon / 2) ** 2
#         a = torch.clamp(a, 0.0, 1.0)
#
#         dist_matrix = 2 * torch.atan2(torch.sqrt(a), torch.sqrt(1 - a))
#         return dist_matrix * R
#
#     def solve(self, agent_indices, coords, use_dist_matrix=True, dist_function=None):
#         """
#         :param agent_indices: (batch_size, n)
#         :param coords: (batch_size, n, 2)
#         :param use_dist_matrix: bool -- (batch_size, n, n)
#         :param dist_function: callable, return distance between two points
#         One of the parameters dist_matrix or dist_function must be not None
#         :return: lengths of max tours (batch_size)
#         """
#         assert use_dist_matrix or dist_function is not None
#
#         batch_size = len(agent_indices)
#         n_depots = agent_indices.max() + 1
#         result = []
#         dist_matrix = None
#         if use_dist_matrix:
#             dist_matrix = self.calculate_distance_matrix_(coords)
#
#         for g_id in range(batch_size):
#             max_length = 0
#             for tsp_id in range(n_depots):
#                 ids = torch.argwhere(agent_indices[g_id] == tsp_id).reshape(-1)
#                 if len(ids) == 0:
#                     continue
#                 if len(ids) < 3:
#                     route = list(range(1, len(ids) + 1))
#                 else:
#                     lkh_coords = coords[g_id] * 10000.0
#                     nodes = {i + 1: tuple(lkh_coords[j].tolist()) for i, j in enumerate(ids)}
#
#                     problem = lkh.LKHProblem(dimension=len(nodes), node_coords=nodes, **self.const_params)
#                     route = lkh.solve(solver=self.solver_path, problem=problem)[0]
#                 route.append(route[0])
#                 route = ids[torch.tensor(route, dtype=torch.int32) - 1]
#                 if use_dist_matrix:
#                     max_length = max(max_length, dist_matrix[g_id][route[:-1], route[1:]].sum().item())
#                 else:
#                     length = 0
#                     for i in range(len(route) - 1):
#                         length += dist_function(coords[g_id, route[i]], coords[g_id, route[i + 1]])
#                     max_length = max(max_length, length)
#             result.append(max_length)
#         return torch.Tensor(result, device=agent_indices.device)

class TSPSolver:
    def __init__(self, edge_weight_type="EUC_2D", solver_path="LKH", max_workers=None):
        self.edge_weight_type = edge_weight_type
        self.const_params = {"name": "TSP_Task", "type": "TSP", "edge_weight_type": edge_weight_type}
        self.solver_path = solver_path
        self.max_workers = max_workers

    def calculate_distance_matrix_(self, coords):
        if self.edge_weight_type == "EUC_2D":
            return torch.cdist(coords, coords)
        elif self.edge_weight_type != "GEO_2D":
            raise NotImplementedError

        R = 6371.0
        coords_rad = coords * (math.pi / 180.0)
        lat = coords_rad[:, :, 0]
        lon = coords_rad[:, :, 1]

        lat_i = lat[:, :, None]
        lat_j = lat[:, None]
        dlat = lat_j - lat_i
        dlon = lon[:, None] - lon[:, :, None]

        a = torch.sin(dlat / 2) ** 2 + torch.cos(lat_i) * torch.cos(lat_j) * torch.sin(dlon / 2) ** 2
        a = torch.clamp(a, 0.0, 1.0)

        dist_matrix = 2 * torch.atan2(torch.sqrt(a), torch.sqrt(1 - a))
        return dist_matrix * R

    def _solve_single_route(self, g_id, ids, coords_g, dist_matrix_g, use_dist_matrix, dist_function):
        if len(ids) < 3:
            route = list(range(1, len(ids) + 1))
        else:
            lkh_coords = coords_g * 10000.0
            nodes = {i + 1: tuple(lkh_coords[j].tolist()) for i, j in enumerate(ids)}
            params = self.const_params.copy()
            params["name"] = f"TSP_batch{g_id}_node{ids[0].item()}"

            problem = lkh.LKHProblem(dimension=len(nodes), node_coords=nodes, **params)
            route = lkh.solve(solver=self.solver_path, problem=problem)[0]

        route.append(route[0])
        route_tensor = ids[torch.tensor(route, dtype=torch.long) - 1]

        if use_dist_matrix:
            length = dist_matrix_g[route_tensor[:-1], route_tensor[1:]].sum().item()
        else:
            length = 0
            for i in range(len(route_tensor) - 1):
                length += dist_function(coords_g[route_tensor[i]], coords_g[route_tensor[i + 1]])

        return g_id, length

    def solve(self, agent_indices, coords, use_dist_matrix=True, dist_function=None):
        assert use_dist_matrix or dist_function is not None

        batch_size = len(agent_indices)
        n_depots = agent_indices.max() + 1

        dist_matrix = None
        if use_dist_matrix:
            dist_matrix = self.calculate_distance_matrix_(coords)

        max_lengths = [0.0] * batch_size

        with ThreadPoolExecutor(max_workers=self.max_workers) as executor:
            futures = []

            for g_id in range(batch_size):
                for tsp_id in range(n_depots):
                    ids = torch.argwhere(agent_indices[g_id] == tsp_id).reshape(-1)
                    if len(ids) == 0:
                        continue

                    coords_g = coords[g_id]
                    dist_matrix_g = dist_matrix[g_id] if use_dist_matrix else None

                    future = executor.submit(
                        self._solve_single_route,
                        g_id, ids, coords_g, dist_matrix_g, use_dist_matrix, dist_function
                    )
                    futures.append(future)

            for future in as_completed(futures):
                g_id, length = future.result()
                if length > max_lengths[g_id]:
                    max_lengths[g_id] = length

        return torch.tensor(max_lengths, device=agent_indices.device)

# class SurrogateNetwork(nn.Module):
#     def __init__(self, n_depots, graph_embed_size, n_layers=3, hidden_size=256):
#         super().__init__()
#
#         assert n_layers >= 2
#         layers = [nn.Linear(2 * n_depots * graph_embed_size, hidden_size), nn.Tanh()]
#         for _ in range(n_layers - 2):
#             layers.extend([nn.Linear(hidden_size, hidden_size), nn.Tanh()])
#         layers += [nn.Linear(hidden_size, 1)]
#         self.layers = nn.Sequential(*layers)
#
#     def forward(self, logits, vertices_embeds, depots):
#         """
#         :param logits: (batch_size, n_depots, n)
#         :param vertices_embeds: (batch_size, n, graph_embed_size)
#         :param depots: (batch_size, n_depots)
#         :return: predicted max length of tsp (batch_size)
#         """
#         batch_size = len(logits)
#         probs = torch.softmax(logits, dim=1)
#         soft_attention = torch.bmm(probs, vertices_embeds) # (batch_size, n_depots, graph_embed_size)
#         depots_embeds = vertices_embeds[torch.arange(batch_size)[:, None], depots] # (batch_size, n_depots, graph_embed_size)
#         routes_features = torch.cat([soft_attention, depots_embeds], dim=-1).reshape(batch_size, -1)
#
#         return self.layers(routes_features).reshape(-1)

class SurrogateNetwork(nn.Module):
    def __init__(self, n_customers, graph_embed_size, n_layers=3, hidden_size=256):
        super().__init__()

        assert n_layers >= 2
        layers = [nn.Linear(n_customers, hidden_size), nn.Tanh()]
        for _ in range(n_layers - 2):
            layers.extend([nn.Linear(hidden_size, hidden_size), nn.Tanh()])
        layers += [nn.Linear(hidden_size, 1)]
        self.layers = nn.Sequential(*layers)

    def forward(self, log_probs):
        """
        :param logits: (batch_size, n_depots, n)
        :param vertices_embeds: (batch_size, n, graph_embed_size)
        :param depots: (batch_size, n_depots)
        :return: predicted max length of tsp (batch_size)
        """

        return self.layers(log_probs).reshape(-1)


class UpperLevelIMTSP(nn.Module):
    def __init__(self, n, n_depots, in_size=3, n_message_layers=3, n_surrogate_layers=3, graph_embed_size=64,
                 agent_embed_size=16, alloc_hidden_size=128, surrogate_hidden_size=256, clip_constant=10, dropout=0.3):
        super().__init__()

        self.gnn = GraphEmbeddings(in_size, graph_embed_size, n_message_layers)
        self.alloc_network = AllocationNetwork(graph_embed_size, agent_embed_size, alloc_hidden_size, clip_constant,
                                               dropout)
        self.surrogate_network = SurrogateNetwork(n - n_depots, graph_embed_size, n_surrogate_layers, surrogate_hidden_size)

    def forward(self, coords, depots):
        """
        :param coords: torch.Tensor coordinates of the vertices (batch_size, n, 2)
        :param depots: torch.LongTensor indices of depots (batch_size, n_depots)
        :return: torch.Tensor logits -- (batch_size, n_depots, n), torch.Tensor predicted_lengths -- (batch_size)
        """
        device = coords.device
        batch_size, n, _ = coords.shape
        n_depots = depots.shape[1]

        full_graph_matrix = torch.argwhere(torch.ones((n, n)) - torch.eye(n)).to(device)
        range_gnn = torch.arange(0, n * batch_size, n, device=device)
        edge_index = (range_gnn[:, None, None] + full_graph_matrix[None]).reshape(n * (n - 1) * batch_size, 2).T
        depots_gnn = torch.zeros((batch_size, n), device=device)
        depots_gnn[torch.arange(batch_size, device=device)[:, None], depots] = True
        batch_gnn = torch.arange(batch_size, device=device).repeat_interleave(n)

        vertices_embeds, graph_context = self.gnn(coords.reshape(n * batch_size, 2), edge_index, depots_gnn.reshape(-1), batch_gnn)
        vertices_embeds = vertices_embeds.reshape(batch_size, n, -1)
        logits = self.alloc_network(vertices_embeds, graph_context, depots)
        dist = torch.distributions.Categorical(logits=logits.transpose(1, 2).reshape(batch_size * n, n_depots))
        agent_indices = dist.sample()
        log_probs = dist.log_prob(agent_indices).reshape(batch_size, n)  # (batch_size, n)
        agent_indices = agent_indices.reshape(batch_size, n)
        mask = torch.ones(batch_size, n, dtype=torch.bool, device=coords.device)
        mask[torch.arange(batch_size)[:, None], depots] = False

        customer_log_probs = log_probs[mask].reshape(batch_size, n - n_depots)

        predicted_lengths = self.surrogate_network(customer_log_probs)

        return customer_log_probs, predicted_lengths, agent_indices
