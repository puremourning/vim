let s:script_var = 'testing inside'

echom "Testing inside"

function! 
      \ Test4(
      \   that,
      \   has,
      \   lots,
      \   of,
      \   lines )
  for i in range( 0, 10 )
    echom "Test4" i
  endfor
endfunction

function! Test3()
	echom "Test3"
        echom a
endf

function! s:Recover( v ) abort
  echom "Recover " a:v
endfunction

try
  call Test3()
catch /this_will_not_match/
  echom "This should not happen"
catch /.*/
  echom "Exception was caught" v:exception
  call s:Recover( s:script_var )
endtry
