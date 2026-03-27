import json
import matplotlib.pyplot as plt

def plot_coordinates(json_file_path, output_image_path):
    try:
        # Загружаем данные из JSON
        with open(json_file_path, 'r') as f:
            data = json.load(f)

        # Извлекаем массив координат
        # Предполагаем структуру {"coordinates": [[x1, y1], [x2, y2], ...]}
        coords = data.get('coordinates', [])

        if not coords:
            print("Массив координат пуст или отсутствует.")
            return

        # Разделяем координаты на X и Y для отрисовки
        x_values = [p[0] for p in coords]
        y_values = [p[1] for p in coords]

        # Настройка графика
        plt.figure(figsize=(10, 8))

        # Рисуем линию и точки
        plt.scatter(x_values, y_values, marker='o', linestyle='-', color='b', label='Path')

        # Добавляем оформление
        plt.xlabel('X')
        plt.ylabel('Y')
        plt.grid(True, linestyle='--', alpha=0.7)
        plt.axis('equal')  # Чтобы масштаб X и Y был одинаковым
        plt.legend()

        # Сохранение в файл
        plt.savefig(output_image_path, dpi=300, bbox_inches='tight')
        plt.close()

        print(f"График успешно сохранен: {output_image_path}")

    except FileNotFoundError:
        print("Файл JSON не найден.")
    except Exception as e:
        print(f"Произошла ошибка: {e}")

# Пример использования
if __name__ == "__main__":
    # Укажите путь к вашему файлу и имя выходного изображения
    plot_coordinates('pcb3038_mdmtsp.json', 'coordinates_plot.png')