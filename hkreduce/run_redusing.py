from typing import Any

import numpy as np
from cantera import Solution  # type: ignore[import-untyped]


def drg_run(st: Solution, state: Any) -> None:
    # Данные взятые из canter'ы:
    #
    # st.net_rates_of_progress - Вектор коэффициентов скорости реакций для заданного состояния.
    # Размерность вектора: кол-во реакция
    #
    # st.product_stoich_coeffs - Двумерный массив (матрица) стехиометрических коэффициентов (тип double) продуктов (те
    # коэффициентов соответствующих правым частям хим. уравнений).
    # Размерность: кол-во веществ * кол-во реакций
    #
    # st.reactant_stoich_coeffs - Аналогично, только для реагентов (те коэфф. левых частей уравнений).
    #
    # st.n_species - Кол-во веществ

    # st.net_rates_of_progress != 0 - Битовая маска, указывающая какие скорости реакций != 0.
    # Размерность та же что и у st.net_rates_of_progress.
    #
    # valid_reactions = np.where(st.net_rates_of_progress != 0) - Массив (вектор) индексов, в которых значения == true.
    # Те массив индексов реакций, скорость которых != 0. Такие реакции дальше называются валидными.
    valid_reactions = np.where(st.net_rates_of_progress != 0)

    # product_stoich_coeffs = st.product_stoich_coeffs[:, valid_reactions] - Двумерный массив (матрица)
    # стехиометрических коэффициентов продуктов, но только для валидных реакций.
    # Размерность: кол-во веществ * кол-во валидных реакций
    #
    # [:, ... - указывает взять по 0 оси все элементы (все строки) (те взять для всех веществ их векторы
    # стехиометрических коэффициентов, соответствующих правым частям хим. уравнений).
    # ..., valid_reactions] - взять по 1 для каждой строки оси только те элементы (столбцы),
    # индексы которых есть в valid_reactions массиве
    # (те взять для каждого вещества коэффициенты, которые относятся к валидным реакциям).
    product_stoich_coeffs = st.product_stoich_coeffs[:, valid_reactions]
    # reactant_stoich_coeffs = st.reactant_stoich_coeffs[:, valid_reactions] - аналогично только для реагентов
    reactant_stoich_coeffs = st.reactant_stoich_coeffs[:, valid_reactions]
    # stoich_coeffs - Итоговый двумерный знаковый массив стехиометрических коэффициентов для валидных реакций
    stoich_coeffs = product_stoich_coeffs - reactant_stoich_coeffs

    # reactions_rate_coeffs = st.net_rates_of_progress[valid_reactions] - Вектор коэфф. скоростей валидных реакций.
    reactions_rate_coeffs = st.net_rates_of_progress[valid_reactions]
    # Удаляем для освобождения памяти
    del valid_reactions

    # stoich_coeffs * reactions_rate_coeffs - как умножение матрицы на вектор, только поэлементное
    # (те без финальной операции сложения по строке).
    # Тем самым получим двумерный массив νAi * ωi с размерностью равной кол-во веществ * кол-во валидных реакций.
    # np.abs - просто поэлементное применение модуля
    base_rates = np.abs(stoich_coeffs * reactions_rate_coeffs)
    del stoich_coeffs, reactions_rate_coeffs

    # delta - Двумерный битовый массив, указывающий в каких валидных реакциях используется вещество.
    # Размерность: кол-во веществ * кол-во валидных реакций.
    # Те это двум. массив δBi.
    # ... | ... - поэлементное битовый OR.
    delta = (product_stoich_coeffs != 0) | (reactant_stoich_coeffs != 0)
    del product_stoich_coeffs, reactant_stoich_coeffs

    # np.sum(base_rates, axis=1) - Просуммирует по 1 оси все элементы (по валидным реакциям), тем самым получим
    # вектор знаменателей (из формулы для DRG) для каждого вещества.
    divider = np.sum(base_rates, axis=1)

    for specy_a in range(st.n_species):
        if divider[specy_a] == 0:
            continue

        # specy_a - Индекс вещества A.
        #
        # base_rates[specy_a] - Вектор |νAi * ωi| для вещества A. Размерность: кол-во валидных реакций.
        #
        # delta * base_rates[specy_a] - Двумерный массив |νAi * ωi| * δBi.
        # delta - Матрица в которой строка соответствует веществу B, элементы же этой строки соответствуют
        # |νAi * ωi| * δBi.
        # Размерность: кол-во веществ * кол-во валидных реакций
        #
        # coefs_for_specy_a = np.sum(...) - Вектор rAB, где A для фиксировано.
        # Тем самым получили все коэфф. матрицы смежности для вещества A, те строку этой матрицы.
        coefs_for_specy_a = np.dot(delta, base_rates[specy_a]) / divider[specy_a]

        # Зануляем rAA
        coefs_for_specy_a[specy_a] = 0

        # add to csr matrix in c layer


def drgep_run(st: Solution, state: Any) -> None:
    # См. комментарии drg_run
    valid_reactions = np.where(st.net_rates_of_progress != 0)
    product_stoich_coeffs = st.product_stoich_coeffs[:, valid_reactions]
    reactant_stoich_coeffs = st.reactant_stoich_coeffs[:, valid_reactions]
    stoich_coeffs = product_stoich_coeffs - reactant_stoich_coeffs

    reactions_rate_coeffs = st.net_rates_of_progress[valid_reactions]
    del valid_reactions

    base_rates_not_abs = stoich_coeffs * reactions_rate_coeffs
    del stoich_coeffs, reactions_rate_coeffs

    # np.sum(base_rates_not_abs, axis=1, where=base_rates_not_abs > 0) - Складывает все положительные элементы
    # по строкам. Получим вектор коэффициентов PA для каждого A.
    pa = np.sum(base_rates_not_abs, axis=1, where=base_rates_not_abs > 0)
    # Аналогично, только разница в том, что суммируем только отрицательные элементы, и только потом
    # домножим на -1.
    ca = np.sum(base_rates_not_abs, axis=1, where=base_rates_not_abs < 0)
    # Делаем через *=, чтобы изменить уже готовый массив
    ca *= -1

    delta = (product_stoich_coeffs != 0) | (reactant_stoich_coeffs != 0)
    del product_stoich_coeffs, reactant_stoich_coeffs

    for specy_a in range(st.n_species):
        divider = max(pa[specy_a], ca[specy_a])
        if divider == 0:
            continue

        coefs_for_specy_a = np.abs(np.sum(delta * base_rates_not_abs[specy_a], axis=1))
        coefs_for_specy_a /= divider

        coefs_for_specy_a[specy_a] = 0

        # add to csr matrix in c layer


def pfa_run(st: Solution, state: Any) -> None:
    valid_reactions = np.where(st.net_rates_of_progress != 0)
    product_stoich_coeffs = st.product_stoich_coeffs[:, valid_reactions]
    reactant_stoich_coeffs = st.reactant_stoich_coeffs[:, valid_reactions]
    stoich_coeffs = product_stoich_coeffs - reactant_stoich_coeffs

    reactions_rate_coeffs = st.net_rates_of_progress[valid_reactions]
    del valid_reactions

    base_rates_not_abs = stoich_coeffs * reactions_rate_coeffs
    del stoich_coeffs, reactions_rate_coeffs

    pa = np.sum(base_rates_not_abs, axis=1, where=base_rates_not_abs > 0)
    ca = np.sum(base_rates_not_abs, axis=1, where=base_rates_not_abs < 0)
    ca *= -1

    delta = (product_stoich_coeffs != 0) | (reactant_stoich_coeffs != 0)
    del product_stoich_coeffs, reactant_stoich_coeffs

    rab_pro_1 = np.empty((st.n_species, st.n_species), dtype=np.float64)
    rab_con_1 = np.empty((st.n_species, st.n_species), dtype=np.float64)

    for specy_a in range(st.n_species):
        divider = max(pa[specy_a], ca[specy_a])
        if divider != 0:
            rab_pro_1_for_specy_a = np.dot(delta, np.maximum(base_rates_not_abs[specy_a], 0))
            rab_pro_1_for_specy_a /= divider
            rab_con_1_for_specy_a = np.dot(delta, np.minimum(base_rates_not_abs[specy_a], 0))
            rab_con_1_for_specy_a *= -1 / divider

            rab_pro_1_for_specy_a[specy_a] = 0
            rab_con_1_for_specy_a[specy_a] = 0

            rab_pro_1[specy_a] = rab_pro_1_for_specy_a
            rab_con_1[specy_a] = rab_con_1_for_specy_a
        else:
            rab_pro_1[specy_a] = 0
            rab_con_1[specy_a] = 0

    rab_pro_2 = np.dot(rab_pro_1, rab_pro_1)
    rab_con_2 = np.dot(rab_con_1, rab_con_1)

    for specy_a in range(st.n_species):
        rab = rab_pro_1[specy_a] + rab_con_1[specy_a] + rab_pro_2[specy_a] + rab_con_2[specy_a]

        # add to csr matrix in c layer
