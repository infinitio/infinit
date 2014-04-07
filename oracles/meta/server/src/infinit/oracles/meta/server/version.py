class Range:

    """A numeric range."""

    def __init__(self, inf, sup = True):
        """Create a numeric range with the given boundaries

        inf -- the inferior boundary.
        sup -- the superior boundary. If unspecified, equals the
               inferior boundary. If None, there is no upper bound
               to the range (it includes any number superior or
               equal to inf).

        >>> 4 in Range(5)
        False
        >>> 5 in Range(5)
        True
        >>> 6 in Range(5)
        False

        >>> 4 in Range(5, 7)
        False
        >>> 5 in Range(5, 7)
        True
        >>> 6 in Range(5, 7)
        True
        >>> 7 in Range(5, 7)
        True
        >>> 8 in Range(5, 7)
        False
        >>> 42 in Range(5, None)
        True
        """
        if isinstance(inf, Range):
            assert sup is True
            sup = inf.sup()
            inf = inf.inf()
        assert inf is not None
        self.__inf = inf
        if sup is True:
            sup = inf
        self.__sup = sup

    def sup(self):
        return self.__sup

    def inf(self):
        return self.__inf

    def __contains__(self, val):
      """Whether val is included in self."""
      if isinstance(val, Range):
        return val.inf() in self
      sup = (self.__sup is None or val <= self.__sup)
      return val >= self.__inf and sup

    def __eq__(self, rhs):
        return self.__inf == rhs.__inf and self.__sup == rhs.__sup

    def __ge__(self, rhs):
        return self.__inf >= rhs.__sup

    def __gt__(self, rhs):
        return self.__inf > rhs.__sup

    def __str__(self):
        """A visual representation of the range.

        >>> str(Range(5))
        '5'
        >>> str(Range(5, 7))
        '[5, 7]'
        >>> str(Range(5, None))
        '[5, ...]'
        """
        if self.__sup == self.__inf:
            return str(self.__inf)
        elif self.__sup is None:
            return '[%s, ...]' % self.__inf
        return '[%s, %s]' % (self.__inf, self.__sup)

    def __repr__(self):
        if self.__sup == self.__inf:
            return 'Range(%s)' % self.__inf
        elif self.__sup is None:
            return 'Range(%s, None)' % self.__inf
        return 'Range(%s, %s)' % (self.__inf, self.__sup)

class Version:

    def __init__(self, major = None, minor = None, subminor = None):
        assert major is not None or minor is None and subminor is None
        assert minor is not None or subminor is None
        self.__major = major and Range(major)
        self.__minor = minor and Range(minor)
        self.__subminor = subminor and Range(subminor)

    @property
    def major(self):
        return self.__major

    @property
    def minor(self):
        return self.__minor

    @property
    def subminor(self):
        return self.__subminor

    def __str__(self):
        if self.__major is not None:
            if self.__minor is not None:
                if self.__subminor is not None:
                    return '%s.%s.%s' % (self.__major, self.__minor,
                                         self.__subminor)
                else:
                    return '%s.%s' % (self.__major, self.__minor)
            else:
                return '%s' % (self.__major)
        else:
            return 'any version'

    def __contains__(self, other):
      """Whether a version includes another.

      >>> Version(1, 2, 3) in Version(1, 2, 3)
      True
      >>> Version(1, 2, 2) in Version(1, 2, 3)
      False
      >>> Version(1, 2, 4) in Version(1, 2, 3)
      False
      >>> Version(1, 2) in Version(1, 2, 3)
      False
      >>> Version(1, 2, 3) in Version(1, 2)
      True
      >>> Version(1, 3) in Version(1, Range(2, 4))
      True
      >>> Version(1, 2, 3) in Version()
      True
      """
      if self.__major is not None:
        if other.__major is None or \
           not other.__major in self.__major:
          return False
        if self.__minor is not None:
          if other.__minor is None or \
             not other.__minor in self.__minor:
            return False
          if self.__subminor is not None:
            if other.__subminor is None or \
               not other.__subminor in self.__subminor:
              return False
      return True

    def __ge__(self, rhs):
        """Whether a version is greater than another.

        >>> Version(1, 2, 3) >= Version(1, 2, 3)
        True
        >>> Version(1, 2, 4) >= Version(1, 2, 3)
        True
        >>> Version(1, 3, 2) >= Version(1, 2, 3)
        True
        >>> Version(2, 0, 0) >= Version(1, 10, 23)
        True
        >>> Version(1, 2, 3) >= Version(1, 2, 4)
        False
        >>> Version(1, 2, 3) >= Version(1, 3, 2)
        False
        """
        assert self.__major is not None and rhs.__major is not None
        if self.__major == rhs.__major:
            minor = self.__minor or 0
            rhs_minor = rhs.__minor or 0
            if minor == rhs_minor:
                subminor = self.__subminor or 0
                rhs_subminor = rhs.__subminor or 0
                return subminor >= rhs_subminor
            else:
                return minor > rhs_minor
        else:
            return self.__major > rhs.__major
