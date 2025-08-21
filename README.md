<!-- markdownlint-disable MD010 -->
<!--MD010 is a rule to use spaces instead of tabs for indentation which I strongly disagree with-->

# ShellTools

## General

During COVID-19 I created this as a collection of helper functions for remote studying at University.<br>
Over time this project's scope grew wider and wider, to now be a fairly comprehensive customisation of zsh.<br>
Initially this project was hosted on a private server and contained a bunch of sensitive information, all that has been stripped out and moved to a (still privately hosted) Extension while the core of the project (now this repo) was made publicly accessible.<br>
Some Design choices made sense at the time but are kind of legacy at this point, while others only make sense in combiniation with some of my private extensions.

## Requirements

- git >= 2.38, see [Known Issues](#known-issues)
- gcc
- zsh
- [oh-my-zsh](https://github.com/ohmyzsh/ohmyzsh)
- cppcheck (optional, if available will run tests on this codebase on setup/update)
- shellcheck (optional, if available will check this codebase and extensions on setup/update)

## Installation

1) make sure you have oh-my-zsh and gcc installed
2) clone this repo
3) edit your .zshrc file to include at least the following (you can have additional things in there as well, this is just the minimum):

	``` zsh
	ZSH="$HOME/.oh-my-zsh"
	export ZSH
	ZSH_THEME="lukasaldersley"
	. "$ZSH/oh-my-zsh.sh"
	ST_SRC="whereverYouPutThisRepo"
	ST_CFG="$HOME/.shelltools"
	export ST_CFG
	export ST_SRC
	. "$ST_SRC/ZSH/zshrc-loader.sh"
	```

	+ "$HOME/.oh-my-zsh" is the default path for oh-my-zsh.<br>
	If you configured that to use a different path, adjust the path here to match.
	+ obviously replace whereverYouPutThisRepo with the actual path
	+ ST_CFG can really be anything, as long as it can be written to, the above is just a recommendation in this case

4) reload your session (`omz reload` or just start a new shell)
	+ This *should* automatically install/set up ShellTools.
	+ If not (or if you want to update ShellTools) run `UpdateZSH` or `uz` to fetch the repo and recompile everything.
5) enjoy

## Configuration

Shelltools can be configured with the file `$ST_CFG/config.cfg`.<br>
If that file doesn't exist, it will be automatically created from [DEFAULTCONFIG.cfg](DEFAULTCONFIG.cfg).<br>
Once `$ST_CFG/config.cfg` exists ShellTools will *not* change/update it.<br>
Should new configuration options become availabe it will be up to you to add those to your config file.<br>
It is not mandatory to have all available configuration options present in your config file. For any missing options the default values (as seen in `DEFAULTCONFIG.cfg`) will be used implicitly.<br>
The config file contains it's own documentation as comments. Please make sure to read those carefully.<br>
When updating your local config file with newly introduced options, it is not mandatory to also update the explanatory comments, but it is highly advisable.

## Extensions

ShellTools supports Extensions.<br>
If you installed ShellTools in `~/TOOLS/ShellTools`, Extensions can be placed in `~/TOOLS`.<br>
ShellTools will look for files named `CUSTOM.sh` and `ShellToolsExtensionLoader.sh` in ShellTools's parent directory and source them as part of it's startup.<br>
ShellToolsExtensionLoader.sh is used as the entry point for my non-public extensions and is tracked in those repos, CUSTOM.sh is meant for general extensions or machine-specific adjustments and is set to be ignored by git in my extension repos.<br>
You are free to use ShellToolsExtensionLoader.sh and CUSTOM.sh as you see fit, my usecase for those is just described for reference<br><br>
*Keep in mind, those files aren't strictly executed, but they are sourced, i.e. loaded into ShellTools's context.*

## Known Issues

If you are on a system with a git version < 2.38.0, you need to add a line with the text `TEMP_OVERRIDE_OLD_GIT_VERSION` anywhere in the config file.<br>
This will make ShellTools use a slightly less optimized function, without this option the readout of what branches are merged and which are not will not work.<br>
If you don't want git integration in the first place, you can set `REPOTOOLS.PROMPT.GIT.ENABLE` to false in the config file. This removes the necessity to have git in the first place outright (though auto update would not function)

<!-- markdownlint-enable MD010 -->
