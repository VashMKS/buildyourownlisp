; computes the absolute value of the difference between x and y
(fun {multiline_function x y} {
    if (== x y)
    {0}
    {
        if (> x y)
        {- x y}
        {- y x}
    }
})
