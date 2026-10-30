"""Limits for class shapes and the repository layout used by the layering check."""

CLASS_LIMITS = {
    'public_methods': 7,
    'public_data': 0,
    'single_method_interface': 0,
}

# Advisory floors: new or changed values above these (and within the limit) are reported.
ADVISORY = {'public_methods': 5}

HINTS = {
    'public_methods': 'split the class or move helpers into free functions',
    'public_data': 'make data private, or split it into a plain struct without methods',
    'single_method_interface': 'prefer a callable (MoveOnlyFunction / std::function) '
                               'over a single-method interface',
}
