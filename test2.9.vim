vim9script

const script_var = 'testing inside'

echom "Testing inside"

export def Test4(
      \   that: string,
      \   has: string,
      \   lots: string,
      \   of: string,
      \   lines: string )
  for i in range( 0, 10 )
    echom "Test4" i
  endfor
enddef

def Test3()
  echom "Test3"
  echom a
enddef

def Recover( v: string )
  echom "Recover " v
enddef

try
  Test3()
catch /this_will_not_match/
  echom "This should not happen"
catch /.*/
  echom "Exception was caught" v:exception
  Recover( script_var )
endtry
