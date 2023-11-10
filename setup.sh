#!/bin/sh

#NOTE this is broken in this public version since I stripped a bunch of private info needed to make this work. I'll fix it eventually

if ! grep -Eq "ID(_LIKE)?=debian" </etc/os-release ; then
	printf "ERROR: only debian-based systems are supported, yours (%s) doesn't seem like one.\n" "$(grep -E "^ID(_LIKE)?" < /etc/os-release |tr '\n' ','|rev|cut -c2-|rev)"
	printf "You may still use these tools, but none of it will be set up automatically.\nCheck this file (%s) to see what would have been done.\n" "$0"
	exit 1
fi

{ WSL_VERSION="$(/mnt/c/Windows/System32/wsl.exe -l -v | iconv -f unicode|grep -E "\b$WSL_DISTRO_NAME\s+Running" | awk '{print $NF}' | cut -c-1)"; } 2> /dev/null || true
if [ "$WSL_VERSION" -eq 2 ] ; then
	printf "You are running %s as WSL 2. are you REALLY sure you wouldn't rather have WSL 1?\nWSL1 is way way faster if you also need to access files from windows which is the entire point of WSL, else you'd just use a VM or a seperate Linux system\nIf so select no here, exit wsl and in Powershell run the following commands\n\n\twsl --set-default-version 1\n\twsl --set-version %s 1\n\nDo you really want to continue in WSL2? if so enter 'yes' " "$WSL_DISTRO_NAME" "$WSL_DISTRO_NAME"
	read -r versionconfirm
	case $versionconfirm in
		yes )
			printf "I'll let you continue but I don't recommend it"
			;;
		* )
			echo "aborting"
			exit 0
			;;
	esac
fi

echo "Executing sudo apt update:"
sudo apt update
TARGET_LIST="zsh git wget"
TO_INSTALL=""
for APPL in $TARGET_LIST ; do
	if ! dpkg -s "$APPL" 2>/dev/null | grep -q "Status: install ok installed" ; then
		#echo "$APPL is not installed"
		TO_INSTALL=" $APPL$TO_INSTALL"
	#else
		#echo "$APPL IS installed"
	fi
done
echo "required to install:$TO_INSTALL"
cmd="sudo apt install -y$TO_INSTALL"
sh -c "$cmd"
if [ ! -e "$HOME/.oh-my-zsh" ]; then
	sh -c "$(wget https://raw.githubusercontent.com/ohmyzsh/ohmyzsh/master/tools/install.sh -O -)" "" --unattended
fi
SSH_KEYFILE="$HOME/.ssh/id_rsa"
if [ ! -f "$SSH_KEYFILE" ]; then
	printf "do you wish to generate a new SSH Key at %s?. (y/n).\nYou can also copy an existing key there and select no." "$SSH_KEYFILE"
	read -r keygen
	case $keygen in
		[Yy][Ee][Ss] | [Yy])
			ssh-keygen -b4096 -trsa -f "$SSH_KEYFILE"
			;;
		* )
			echo "skipping ssh key file generation"
			;;
	esac
fi
cat "$SSH_KEYFILE.pub"
printf "Above is your ssh public key (%s). Add it to any devices/services needed (github/gitlab/etc)\nPRESS ENTER KEY WHEN DONE" "$SSH_KEYFILE.pub"
read -r CL
printf ".headers on\n.changes on\n.mode table\n" > "$HOME/.sqliterc"

if [ ! -e "$HOME/.gdb-peda/.git" ] ; then
	PEDA_INST="$HOME/.gdb-peda"
	git clone https://github.com/longld/peda.git "$PEDA_INST"
	printf "source %s/peda.py" "$PEDA_INST" >> .gdbinit
fi

ZSH_TL="gcc g++ gdb openssh-server qalc cppcheck curl man-db htop util-linux sqlite3 netcat-openbsd file"
ZSH_TO_INSTALL=""
for APPL in $ZSH_TL ; do
	if ! dpkg -s "$APPL" 2>/dev/null | grep -q "Status: install ok installed" ; then
		#echo "$APPL is not installed"
		ZSH_TO_INSTALL=" $APPL$ZSH_TO_INSTALL"
	#else
		#echo "$APPL IS installed"
	fi
done
echo "recomend to install:$ZSH_TO_INSTALL"
cmd="sudo apt install$ZSH_TO_INSTALL"
zsh -ic "$cmd; if [ \$? -eq 0 ] ; then echo \"done\"; exit ; else printf \"something went wrong when calling %s, here is an iteractive shell to fix it, when done, exit, so the setup process can continue\n\" $cmd; zsh -i ; echo \"continuing\"; fi"
zsh -ic "UpdateZSH;code 2>/dev/null;exit"
#this is a bit unsatisfying...
#if vs code isn't installed, install it
#if ! which inexistantcommand | grep -vq 'not found' ; then echo "INSTALL" ; else echo "DONT" ; fi
if ! which code | grep -vq 'not found' ; then
	if [ "$(uname -m)" != "x86_64" ]; then
		echo "since you aren't using x86_64 VS Code will NOT be Auto-Installed"
	else
		echo "getting/installing vs code" ;
		curl -L -o vsc.deb "https://code.visualstudio.com/sha/download?build=stable&os=linux-deb-x64"
		sudo dpkg -i vsc.deb
		rm vsc.dpkg
	fi
else
        echo "'code' is already installed -> skipping"
fi
sudo apt update
sudo apt upgrade
zsh -ic 'printf "\e[38;5;009mif you want to use zsh as your default shell, run '\''chsh -s /usr/bin/zsh'\''\e[0m\n - starting zsh as a \e[38;5;009mone time\e[0m run\n" ; UpdateZSH'
