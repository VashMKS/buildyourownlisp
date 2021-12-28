;;; Test helpers

; In order to be able to implement useful test functions some language features are needed, like:
; - better string manipulation to help printing
; - more OS interaction like file opening/reading
; - assertions
; - better introspection/namespacing (e.g. I want code to be able to execute all tests)

;;; Atoms
if (!= nil {})
	{error "nil atom should equal empty list {}"}
	{test_passed}

;;; Functions

;;; Conditionals / Flow control

;;; Lists

;;; Math

