breakadd file 14 test.vim

function! s:Test()
  let test = 'this is a test'
  let l:another_test = #{ key: 'value', test: 'testing' }
  let list_test = [ 'a', 'b', 1, 2, #{ one: 'two' } ]
  echom test
  " This is a comment
  echom "test2"
  source test2.vim
endfunction

function! Test2( a )
  echom a:a
  call s:Test()
endfunction

let s:foooooooo = 'fuuuuuu'
let g:f = 'f' .. s:foooooooo

call Test2( g:f )
