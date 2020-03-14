let s:script_var = 'testing inside'

echom "Testing inside"

function! Test3()
	echom "Test3"
        echom a
endf

function! s:Recover( v ) abort
  echom "Recover " a:v
endfunction

try
  call Test3()
catch /.*/
  echom "Exception was caught" v:exception
  call s:Recover( s:script_var )
endtry
