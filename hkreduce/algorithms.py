# ruff: noqa: N806

import numpy as np
from cantera import Solution  # type: ignore[import-untyped]
from loguru import logger
from numpy.typing import NDArray

from .cpp_interface import CSRAdjacencyMatrix
from .typing import PathLike
from .utils import NumpyArrayDumper, create_unique_file


def create_matrix_for_drg(  # type: ignore[return] # noqa: C901
    st: Solution,
    temperature: float,
    pressure: float,
    mass_fractions: NDArray[np.float64],
    *,
    save: bool,
    tmp_dir: PathLike,
    ai_cond_idx: int,
    state_idx: int,
) -> CSRAdjacencyMatrix:
    st.TPY = (temperature, pressure, mass_fractions)

    if save:
        logger.trace(
            "Save net rates for {state_idx} state of {ai_cond_idx} case", state_idx=state_idx, ai_cond_idx=ai_cond_idx
        )
        with NumpyArrayDumper(
            dir=tmp_dir,
            filename=create_unique_file(
                dir=tmp_dir,
                prefix=f"net_rates_of_progress_for_{state_idx}_state_of_{ai_cond_idx}_ai_cond_",
                suffix=".npy",
            ).name,
        ).open("w") as saver:
            saver.write_data(st.net_rates_of_progress)

        logger.trace(
            "Save matrix for {state_idx} state of {ai_cond_idx} case", state_idx=state_idx, ai_cond_idx=ai_cond_idx
        )
        saver = NumpyArrayDumper(
            dir=tmp_dir,
            filename=create_unique_file(
                dir=tmp_dir, prefix=f"matrix_for_{state_idx}_state_of_{ai_cond_idx}_ai_cond_", suffix=".npy"
            ).name,
        ).open("w")

    # only consider contributions from reactions with nonzero net rates of progress
    valid_reactions = np.where(st.net_rates_of_progress != 0)[0]

    try:
        csr_matrix = CSRAdjacencyMatrix(st.n_species)
    except Exception as error:
        if save:
            saver.close()
        raise RuntimeError("Exception from c++ layer") from error

    if valid_reactions.size:
        product_stoich_coeffs = st.product_stoich_coeffs[:, valid_reactions]
        reactant_stoich_coeffs = st.reactant_stoich_coeffs[:, valid_reactions]
        net_stoich = reactant_stoich_coeffs - product_stoich_coeffs

        flags = (product_stoich_coeffs != 0) | (reactant_stoich_coeffs != 0)

        del product_stoich_coeffs, reactant_stoich_coeffs

        base_rates = np.abs(net_stoich * st.net_rates_of_progress[valid_reactions])

        del valid_reactions, net_stoich

        denominator = np.sum(base_rates, axis=1)[:, np.newaxis]

        numerator = np.empty((st.n_species, st.n_species), dtype=np.float64)
        for specy_b in range(st.n_species):
            numerator[:, specy_b] = np.sum(base_rates[:, np.where(flags[specy_b])[0]], axis=1)

        del base_rates, flags

        # May get divide by zero if an inert species is present, and denominator
        # entry is zero.
        with np.errstate(divide="ignore", invalid="ignore"):
            adjacency_matrix = np.where(denominator != 0, numerator / denominator, 0)

        del denominator, numerator

        np.fill_diagonal(adjacency_matrix, 0.0)

        if save:
            saver.write_data(adjacency_matrix)

        try:
            for specy_a in range(st.n_species):
                csr_matrix.add_row(adjacency_matrix[specy_a], specy_a)
        except Exception as error:
            if save:
                saver.close()
            raise RuntimeError("Exception from c++ layer") from error

    elif save:
        saver.write_data(np.zeros((st.n_species, st.n_species), dtype=np.float64))

    try:
        csr_matrix.finalize()
    except Exception as error:
        if save:
            saver.close()
        raise RuntimeError("Exception from c++ layer") from error

    if save:
        saver.close()

    return csr_matrix


def create_matrix_for_drgep(  # type: ignore[return] # noqa: C901
    st: Solution,
    temperature: float,
    pressure: float,
    mass_fractions: NDArray[np.float64],
    *,
    save: bool,
    tmp_dir: PathLike,
    ai_cond_idx: int,
    state_idx: int,
) -> CSRAdjacencyMatrix:
    st.TPY = (temperature, pressure, mass_fractions)

    if save:
        logger.trace(
            "Save net rates for {state_idx} state of {ai_cond_idx} case", state_idx=state_idx, ai_cond_idx=ai_cond_idx
        )
        with NumpyArrayDumper(
            dir=tmp_dir,
            filename=create_unique_file(
                dir=tmp_dir,
                prefix=f"net_rates_of_progress_for_{state_idx}_state_of_{ai_cond_idx}_ai_cond_",
                suffix=".npy",
            ).name,
        ).open("w") as saver:
            saver.write_data(st.net_rates_of_progress)

        logger.trace(
            "Save matrix for {state_idx} state of {ai_cond_idx} case", state_idx=state_idx, ai_cond_idx=ai_cond_idx
        )
        saver = NumpyArrayDumper(
            dir=tmp_dir,
            filename=create_unique_file(
                dir=tmp_dir, prefix=f"matrix_for_{state_idx}_state_of_{ai_cond_idx}_ai_cond_", suffix=".npy"
            ).name,
        ).open("w")

    valid_reactions = np.where(st.net_rates_of_progress != 0)[0]

    try:
        csr_matrix = CSRAdjacencyMatrix(st.n_species)
    except Exception as error:
        if save:
            saver.close()
        raise RuntimeError("Exception from c++ layer") from error

    if valid_reactions.size:
        product_stoich_coeffs = st.product_stoich_coeffs[:, valid_reactions]
        reactant_stoich_coeffs = st.reactant_stoich_coeffs[:, valid_reactions]
        net_stoich = reactant_stoich_coeffs - product_stoich_coeffs

        flags = (product_stoich_coeffs != 0) | (reactant_stoich_coeffs != 0)

        del product_stoich_coeffs, reactant_stoich_coeffs

        base_rates = net_stoich * st.net_rates_of_progress[valid_reactions]

        del valid_reactions, net_stoich

        denominator_dest = np.sum(np.minimum(0.0, base_rates), axis=1) * (-1)
        denominator_prod = np.sum(np.maximum(0.0, base_rates), axis=1)
        denominator = np.maximum(denominator_prod, denominator_dest)[:, np.newaxis]
        del denominator_dest, denominator_prod

        numerator = np.empty((st.n_species, st.n_species), dtype=np.float64)
        for specy_b in range(st.n_species):
            numerator[:, specy_b] = np.sum(base_rates[:, np.where(flags[specy_b])[0]], axis=1)
        numerator = np.abs(numerator)

        del base_rates, flags

        # May get divide by zero if an inert species is present, and denominator
        # entry is zero.
        with np.errstate(divide="ignore", invalid="ignore"):
            adjacency_matrix = np.where(denominator != 0, numerator / denominator, 0)

        del numerator, denominator

        np.fill_diagonal(adjacency_matrix, 0.0)

        if save:
            saver.write_data(adjacency_matrix)

        try:
            for specy_a in range(st.n_species):
                csr_matrix.add_row(adjacency_matrix[specy_a], specy_a)
        except Exception as error:
            if save:
                saver.close()
            raise RuntimeError("Exception from c++ layer") from error

    elif save:
        saver.write_data(np.zeros((st.n_species, st.n_species), dtype=np.float64))

    try:
        csr_matrix.finalize()
    except Exception as error:
        if save:
            saver.close()
        raise RuntimeError("Exception from c++ layer") from error

    if save:
        saver.close()

    return csr_matrix


def create_matrix_for_pfa(  # type: ignore[return] # noqa: C901
    st: Solution,
    temperature: float,
    pressure: float,
    mass_fractions: NDArray[np.float64],
    *,
    save: bool,
    tmp_dir: PathLike,
    ai_cond_idx: int,
    state_idx: int,
) -> CSRAdjacencyMatrix:
    st.TPX = (temperature, pressure, mass_fractions)

    if save:
        logger.trace(
            "Save net rates for {state_idx} state of {ai_cond_idx} case", state_idx=state_idx, ai_cond_idx=ai_cond_idx
        )
        with NumpyArrayDumper(
            dir=tmp_dir,
            filename=create_unique_file(
                dir=tmp_dir,
                prefix=f"net_rates_of_progress_for_{state_idx}_state_of_{ai_cond_idx}_ai_cond_",
                suffix=".npy",
            ).name,
        ).open("w") as saver:
            saver.write_data(st.net_rates_of_progress)

        logger.trace(
            "Save matrix for {state_idx} state of {ai_cond_idx} case", state_idx=state_idx, ai_cond_idx=ai_cond_idx
        )
        saver = NumpyArrayDumper(
            dir=tmp_dir,
            filename=create_unique_file(
                dir=tmp_dir, prefix=f"matrix_for_{state_idx}_state_of_{ai_cond_idx}_ai_cond_", suffix=".npy"
            ).name,
        ).open("w")

    try:
        csr_matrix = CSRAdjacencyMatrix(st.n_species)
    except Exception as error:
        if save:
            saver.close()
        raise RuntimeError("Exception from c++ layer") from error

    valid_reactions = np.where(st.net_rates_of_progress != 0)[0]

    if valid_reactions.size:
        product_stoich_coeffs = st.product_stoich_coeffs[:, valid_reactions]
        reactant_stoich_coeffs = st.reactant_stoich_coeffs[:, valid_reactions]
        net_stoich = reactant_stoich_coeffs - product_stoich_coeffs

        flags = (product_stoich_coeffs != 0) | (reactant_stoich_coeffs != 0)

        del product_stoich_coeffs, reactant_stoich_coeffs

        base_rates = np.array(net_stoich * st.net_rates_of_progress[valid_reactions])

        del net_stoich

        production_A = np.sum(np.maximum(base_rates, 0), axis=1)
        consumption_A = -1 * np.sum(np.minimum(base_rates, 0), axis=1)
        production_AB = np.empty((st.n_species, st.n_species), dtype=np.float64)
        consumption_AB = np.empty((st.n_species, st.n_species), dtype=np.float64)
        for specy_b in range(st.n_species):
            production_AB[:, specy_b] = np.sum(np.maximum(base_rates[:, np.where(flags[specy_b])[0]], 0.0), axis=1)
            consumption_AB[:, specy_b] = -1 * np.sum(
                np.minimum(base_rates[:, np.where(flags[specy_b])[0]], 0.0), axis=1
            )

        del base_rates

        # May get divide by zero if an inert species is present, and denominator
        # entry is zero.
        denominator = np.maximum(production_A, consumption_A)[:, np.newaxis]

        del production_A, consumption_A

        with np.errstate(divide="ignore", invalid="ignore"):
            r_pro_AB1 = np.where(denominator != 0, production_AB / denominator, 0)
        with np.errstate(divide="ignore", invalid="ignore"):
            r_con_AB1 = np.where(denominator != 0, consumption_AB / denominator, 0)

        del production_AB, consumption_AB, denominator

        r_pro_AB2: NDArray[np.float64] | None = None
        r_con_AB2: NDArray[np.float64] | None = None

        for specy_m in range(st.n_species):
            pro1 = r_pro_AB1[:, specy_m]
            pro2 = r_pro_AB1[specy_m, :]
            con1 = r_con_AB1[:, specy_m]
            con2 = r_con_AB1[specy_m, :]
            pro1[specy_m] = 0
            pro2[specy_m] = 0
            con1[specy_m] = 0
            con2[specy_m] = 0
            if r_pro_AB2 is None:
                r_pro_AB2 = np.outer(pro1, pro2)
            else:
                r_pro_AB2 += np.outer(pro1, pro2)
            if r_con_AB2 is None:
                r_con_AB2 = np.outer(con1, con2)
            else:
                r_con_AB2 += np.outer(con1, con2)

        del pro1, pro2, con1, con2

        adjacency_matrix = r_pro_AB1 + r_con_AB1 + r_pro_AB2 + r_con_AB2

        del r_pro_AB1, r_con_AB1, r_pro_AB2, r_con_AB2

        try:
            for specy_a in range(st.n_species):
                csr_matrix.add_row(adjacency_matrix[specy_a], specy_a)
        except Exception as error:
            if save:
                saver.close()
            raise RuntimeError("Exception from c++ layer") from error

    elif save:
        saver.write_data(np.zeros((st.n_species, st.n_species), dtype=np.float64))

    try:
        csr_matrix.finalize()
    except Exception as error:
        if save:
            saver.close()
        raise RuntimeError("Exception from c++ layer") from error

    if save:
        saver.close()

    return csr_matrix
