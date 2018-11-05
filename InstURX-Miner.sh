#!/bin/bash
echo " "
jeshile='\e[40;38;5;82m' #jeshile
echo " "
echo -e "${jeshile} ┌──────────────────────────────────────────┐ \e[0m"
echo -e "${jeshile} │     Installing necessary components      │ \e[0m"
echo -e "${jeshile} └──────────────────────────────────────────┘ \e[0m"
echo " "
apt-get update -y
apt-get install -y build-essential libssl-dev libcurl4-openssl-dev libjansson-dev libgmp-dev automake screen zlib1g-dev git
echo " "
echo -e "${jeshile} ┌──────────────────────────────────────────┐ \e[0m"
echo -e "${jeshile} │              Building Miner              │ \e[0m"
echo -e "${jeshile} └──────────────────────────────────────────┘ \e[0m"
echo " "
cd ~
if [ -d "cpuminer-opt" ]; then
  mv cpuminer-opt cpuminer-opt.old
fi
git clone https://github.com/RickillerZ/cpuminer-opt.git
cd cpuminer-opt/
chmod 755 build.sh autogen.sh 
./build.sh
chmod 755 cpuminer
echo " "
echo -e "${jeshile} ┌──────────────────────────────────────────┐ \e[0m"
echo -e "${jeshile} │       Configuring the Miner              │ \e[0m"
echo -e "${jeshile} └──────────────────────────────────────────┘ \e[0m"
echo " "
read -p 'URX WALLET ADRESS (Ex: UbvpoaUyn6qkJNTtMpvHVupABoCjZjcjiS) : ' adress
read -p 'Enter URX Alias (Ex: Aili) : ' alias
read -p 'Number of thread (Ex: 3) : ' thread
echo "cpuminer -a argon2ad -o stratum+tcp://mine.icemining.ca:4234 -u $adress.$alias -p c=URX -t $thread" > mine.sh
chmod 755 mine.sh 
mv cpuminer mine.sh /usr/local/bin/
echo -e "${jeshile} ┌────────────────────────────────────────────────┐ \e[0m"
echo -e "${jeshile} │[+] installation Completed                      │ \e[0m"
echo -e "${jeshile} │[+] type:                                       │ \e[0m"
echo -e "${jeshile} │#screen                                         │ \e[0m"
echo -e "${jeshile} │#mine.sh                                        │ \e[0m"
echo -e "${jeshile} └────────────────────────────────────────────────┘ \e[0m"
