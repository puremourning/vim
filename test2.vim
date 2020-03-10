let s:script_var = 'testing inside'

echom "Testing inside"

function! Test3()
	echom "Test3"
        echom a
endf

call Test3()
