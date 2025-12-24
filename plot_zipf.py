import pandas as pd
import matplotlib.pyplot as plt
import numpy as np

# 1. Загружаем данные
df = pd.read_csv("zipf_data.csv")

# 2. Подготовка данных для графика
ranks = df["Rank"]
freqs = df["Frequency"]

# 3. Теоретический закон Ципфа: Freq = C / Rank
# C берем как частоту самого популярного слова (rank=1)
C = freqs[0] 
zipf_theoretical = C / ranks

# 4. Построение графика
plt.figure(figsize=(10, 6))

# Реальные данные (синие точки)
plt.loglog(ranks, freqs, marker='.', linestyle='None', markersize=2, label='Реальные данные')

# Теоретическая прямая (красная линия)
plt.loglog(ranks, zipf_theoretical, 'r-', linewidth=2, label='Закон Ципфа (теория)')

plt.title('Распределение терминов (Закон Ципфа)')
plt.xlabel('Ранг (log scale)')
plt.ylabel('Частота (log scale)')
plt.legend()
plt.grid(True, which="both", ls="-", alpha=0.5)

plt.savefig("zipf_plot.png")
plt.show()