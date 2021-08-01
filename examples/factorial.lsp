; Compute the factorial of a natural number n
(fun {fact n} {
     if (== n 0) {1} {* n (fact (- n 1))}
})
