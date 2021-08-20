;;;
;;; LSP STANDARD LIBRARY
;;;

;;; Atoms
(def {nil} {})

;;; Functions

; Function definition (also implemented as part of core, redefined here)
(def {fun} (lambda {formals body} {
	def (head formals) (lambda (tail formals) body)
}))

; Function composition
(fun {comp f g x} {f (g x)})

; Argument swap
(fun {flip f a b} {f b a})

; Currying / uncurrying
(fun {unpack f args} {eval (join (list f) args)})
(fun {pack f & args} {f args})
(def {curry} unpack)
(def {uncurry} pack)

; Open new scope
(fun {let b} {
	((lambda {_} b) ())
})

;;; Logical functions 

; Aliases from builtins
(fun {not x}   {!  x})
(fun {or  x y} {|| x y})
(fun {and x y} {&& x y})

;;; Conditionals / Flow control

; Evaluate several expressions in sequence
(fun {do & commands} {
	if (== commands nil)
		{nil}
		{last commands}
})

; Case selection
; select
; 	{condition value}
;   ...
; 	{condition value}
; 	{otherwise value}
(fun {select & cases} {
	if (== cases nil)
		{error "No selection match"}
		{if (frst (frst cases)) {scnd (frst cases)} {unpack select (tail cases)}}
})

; More traditional switch statement
(fun {case x & cases} {
	if (== cases nil)
		{error "No case match"}
		{if (== x (frst (frst cases)))
			{scnd (frst cases)}
			{unpack case (join (list x) (tail cases))}
		}
})

; default case
(def {otherwise} true)

;;; Lists

; Element selection
(fun {frst l} {eval (head l)})
(fun {scnd l} {eval (head (tail l))})
(fun {thrd l} {eval (head (tail (tail l)))})
(fun {nth n l} {
	if (== n 0) {frst l} {nth (- n 1) (tail l)}
})
(fun {last l} {nth (- (len l) 1) l})

; Take first n elements
(fun {take n l} {
	if (== n 0)
		{nil}
		{join (head l) (take (- n 1) (tail l))}
})

; Drop first n elements
(fun {drop n l} {
	if (== n 0)
		{l}
		{drop (- n 1) (tail l)}
})

; Split at n
(fun {split n l} {list (take n l) (drop n l)})

; Take elements while a condition is met
(fun {take-while f l} {
	if (not (unpack f (head l)))
		{nil}
		{join (head l) (take-while f (tail l))}
})

; Drop elements while a condition is met
(fun {drop-while f l} {
	if (not (unpack f (head l)))
		{nil}
		{drop-while f (tail l)}
})

; Reverse list
(fun {reverse l} {
	if (== l nil) 
		{nil}
		{join (reverse (tail l)) (head l)}
})

; Element membership
(fun {elem x l} {
	if (== l nil)
		{false}
		{if (== x (frst l)) {true} {elem x (tail l)}}
})
; Reimplementation using foldl
; (fun {elem x l} {
;     foldl (lambda {z y} {or z (== x y)}) false l
; })

; Find element in list of pairs
(fun {lookup x l} {
	if (== l nil)
		{error "No element found"}
		{do
			(= {key} (frst (frst l)))
			(= {val} (scnd (frst l)))
			(if (== key x) {val} {lookup x (tail l)})
		}
})

; Zip two lists into a list of pairs
(fun {zip l1 l2} {
	if (or (== l1 nil) (== l2 nil))
		{nil}
		{join (list (join (head l1) (head l2))) (zip (tail l1) (tail l2))}
})

; Unzip a list of pairs into two lists
(fun {unzip l} {
	if (== l nil)
		{{nil nil}}
		{do
			(= {x} (frst l))
		}
})

; Apply function to each element of a list
(fun {map f l} {
	if (== l nil)
		{nil}
		{join (list (f (frst l))) (map f (tail l))}
})

; Apply filter to list
(fun {filter f l} {
	if (== l nil)
		{nil}
		{join (if (f (frst l)) {head l} {nil}) (filter f (tail l))}
})

; Fold left
(fun {foldl f z l} {
	if (== l nil)
		{z}
		{foldl f (f z (frst l)) (tail l)}
})

; Fold right
(fun {foldr f z l} {
	if (== l nil)
		{z}
		{f (frst l) (foldr f z (tail l))}
})

; Basic folds
(fun {sum l}     {foldl + 0 l})
(fun {product l} {foldl * 1 l})

;;; Math

; Minimum of arguments
(fun {min & xs} {
	if (== (tail xs) nil)
		{frst xs}
		{do
			(= {rest} (unpack min (tail xs)))
			(= {item} (frst xs))
			(if (< item rest) {item} {rest})
		}
})

; Maximum of arguments
(fun {max & xs} {
	if (== (tail xs) nil)
		{frst xs}
		{do
			(= {rest} (unpack max (tail xs)))
			(= {item} (frst xs))
			(if (> item rest) {item} {rest})
		}
})

