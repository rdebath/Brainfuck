" Vim syntax file
" Language:     Brainfuck
" Maintainer:   rdebath c/o github.com
" Version:      1.0.20140719

" This file highlights the eight BF command characters in four (reasonable)
" classes. This is the simple part.
"
" In addition it highlights SOME of the other characters as comments.
" Generally it tries (with moderate success) to distinguish between proper
" comments and sequences that are probably supposed to be comments but
" actually contain active BF command characters. In addition it tries to
" identify the 'dead code' style comment loops highlighting any BF command
" characters within the loop in the 'PreProc' style to distinguish them
" from commands that may actually be executed.

" Use the sync options to control how far to search for comment loops.

" For version 5.x: Clear all syntax items
" For version 6.x: Quit when a syntax file was already loaded
if !exists("main_syntax")
  if version < 600
    syntax clear
  elseif exists("b:current_syntax")
    finish
  endif
  let main_syntax = 'brainfuck'
endif

syn sync clear
syn sync minlines=150
syn sync maxlines=150
syn sync linebreaks=25
set synmaxcol=1024

" Various comment marking characters, make them very visible (if not in a comment)
syn match bfError "[{}#!]\+"
syn match bfError "//"
" Only highlight these at start of line
syn match bfError "\(^[	 ]*\)\@<=[;%]"

" Mark the ! as a character of interest as some interpreters react very badly.
syn match bfSPError "!"

" Spaces at EOL
if exists("bf_space_errors")
  syn match bfSPError "[	 ]\+$"
endif

" Basic syntax colouring
syn match bfMath    "[+-]"
syn match bfPointer "[<>]"
syn match bfLoop    "[[\]]"
syn match bfIO      "[.,]"

" Highlight as comment anything following a comment marker that has no commands
syn match bfComment "//[^+\-<>[\].,]*$" contains=bfSPError
" Curly brackets used as internal comment markers
syn match bfComment "{[^+\-<>[\].,{}]*}"
" Generic format of: start of line, bf chars, not bf, end of line
syn match bfComment "\(^[[\]\-+<>.,\t ]*\)\@<=[^[\]\-+<>.,\t ][^[\]\-+<>.,]*$" contains=bfSPError

" Then a 'comment loop'
"
" Any BF command characters within a comment loop are marked differently to BF
" command characters outside a loop. Have to do have the regex parts for the
" beginning of comment loops in the expressions twice because \@<= will only
" search back one line irrespective of the contents of 'sync'.
syn match bfCommentChar "[^+\-<>[\].,]\+" contained contains=bfSPError
syn region bfCommentLoop    start="\["  end="]" contained contains=bfCommentChar,bfCommentLoop,bfSPError
syn match bfComment    "\(]\_[^[\]\-+<>.,]*\)\@<=\_[^[\]\-+<>.,]*\[" contains=bfCommentLoop,bfSPError
syn match bfComment    "\(\%^\_[^[\]\-+<>.,]*\)\@<=\_[^[\]\-+<>.,]*\[" contains=bfCommentLoop,bfSPError

syn match bfCommentLoop	"\]\@<=[^\-<>[\].,{#/]\+\[\-]" contains=bfCommentChar

if version >= 508 || !exists("did_brainfuck_syn_inits")
  if version < 508
    let did_brainfuck_syn_inits = 1
    command -nargs=+ HiLink hi link <args>
  else
    command -nargs=+ HiLink hi def link <args>
  endif

  HiLink bfMath    Constant
  HiLink bfPointer Type
  HiLink bfLoop    Statement
  HiLink bfIO      Special
  HiLink bfError   Error

  HiLink bfCommentChar	Comment
  HiLink bfComment	Comment
  HiLink bfCommentLoop	PreProc
  HiLink bfChars	PreProc


  if !exists("bf_softerr")
    " Initially I did this; but I find it too much of a highlight.
    HiLink bfSPError	Error
  else
    if &bg=="dark"
	" This looks a lot better to me
	highlight Warn ctermbg=22 guibg=Blue
	HiLink bfSPError	Warn
    else
	" HiLink bfSPError	Error
    endif
  endif

  delcommand HiLink
endif

let b:current_syntax = "brainfuck"

if main_syntax == 'brainfuck'
  unlet main_syntax
endif

" vim: ts=8
