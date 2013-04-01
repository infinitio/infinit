setlocal shiftwidth=2 "Normal ident for >>, << and cindent
setlocal tabstop=8 "Number of space that are inserted when <Tab> is pressed
setlocal softtabstop=0 "disable the softtabstop feature
setlocal expandtab "expand tab to space when <Tab> is pressed, (but not for ^V<Tab>)

setlocal comments=sO:*\ -,mO:*\ \ ,exO:*/,s1:/*,mb:*,ex:*/,bO:///,O://
setlocal cinkeys=0{,0},0),:,0#,!^F,o,O,e
setlocal cindent
setlocal cinoptions=ls,g0,Ns,t0,(0
"setlocal cinoptions={1s,}0,>2s,e0,n-1s,f0,:0,^-1s
