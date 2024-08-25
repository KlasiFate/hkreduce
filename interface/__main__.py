from argparse import ArgumentParser


def build_parser() -> ArgumentParser:
    parser = ArgumentParser(
        prog="hcreduce",
        description="Данная программа выполняет редукцию химико-кинетических моделей",
        exit_on_error=True,
        add_help=False
    )

    parser.add_argument(
        "-i",
        "--input",
        action="store",
        type=str,
        required=True,
        dest="filepath",
        help="Путь до файла, описывающий задачу",
    )

    parser.add_argument("-h", "--help", action="help", help="Показывает данное сообщение")

    parser.add_argument(
        "--num_threads",
        action="store",
        type=int,
        default=None,
        dest="num-threads",
        help="Кол-во потоков используемое для моделирования, если не указано, \
то используется кол-во потоков в системе минус 1"
    )

    return parser



def main() -> None:
    build_parser().parse_args()


if __name__ == '__main__':
    main()