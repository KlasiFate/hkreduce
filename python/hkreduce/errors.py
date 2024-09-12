class BaseError(Exception):
    def __init__(self, msg: str, *args) -> None:
        super().__init__(msg, *args)

    @property
    def msg(self) -> str:
        return self.args[0]

    def __str__(self) -> str:
        return self.msg

    def __repr__(self) -> str:
        return f"{self.__class__.__module__}.{self.__class__.__name__}: {self.msg}"


class SimulationError(BaseError):
    pass


class SampleCreatingError(SimulationError):
    pass


class NoAutoignitionError(SimulationError):
    pass


class TooSmallStepsSampleError(SimulationError):
    pass


class ReducingError(BaseError):
    pass


class AutoretrievingInitialThresholdError(ReducingError):
    pass
