function gitupdate {
	git submodule foreach --recursive 'if git symbolic-ref --short HEAD >/dev/null 2>&1 ; then git pull --ff-only --prune ; else echo \"$(pwd) is not on a branch -> fetch-only\" ; git fetch --prune ; fi'
	git pull --ff-only --prune
}

function hist {
	code (Get-PSReadLineOption).HistorySavePath
}
