; Checks if a number is a natural number (0 is a natural)
(fun {is_natural x} {
    if (|| (< x 0) (!= (% x 1) 0))
        {false}
	{true}
})

; Generates a list of integers from 1 to n
(fun {range n} {
    if (<= n 1)
        {list 1}
        {join (range (- n 1)) (list n)}
})

; Compute the factorial n! of a natural number n
(fun {fact n} {
    if (is_natural n)
        {eval (cons * (range n))}
	{error "Can't compute factorial of nonnatural number"}
})
