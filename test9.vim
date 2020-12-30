vim9script
import Test4 from "./test2.9.vim"

def Test()
  const test = 'this is a test'
  const another_test = {
    key: 'value',
    test: 'testting'
  }
  const list_test = [ 'a', 'b', 1, 2, #{ one: 'two' } ]

  echom test

  source test2.9.vim
enddef

def g:Test2( a: string )
  echom a
  s:Test()
enddef

autocmd User TestAutoCommand call s:Test()
autocmd User TestAutoCommand call g:Test2()

Test4( 'that',
       'has',
       'lots',
       'of',
       'lines' )
