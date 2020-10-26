class OnrampException(Exception):
    """ Base Exception for all custom Onramp Exceptions """

    pass


class EmptyDiffException(OnrampException):
    """ Thrown when we encounter an OSC change file with no changes """

    pass
