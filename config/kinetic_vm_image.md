* > yum update
* > yum install gcc kernel-devel vim emacs clang curl net-tools tcpdump git java python lua ruby rubygems bundler openssl openssl-devel valgrind doxygen
* > visudo # enable 'wheel' group and add greg/job/scott to root access
* > useradd --groups wheel greg
* > useradd --groups wheel job
* > useradd --groups wheel scott
* > mkdir git
* > cd git
* > git clone --recursive git@github.com:Seagate/kinetic-c.git
* > cd kinetic-c
* > git checkout develop
* > git pull
* > git submodule update --init
* > gem install bundler
* > bundle install
* > make
* > make clean
* > make all
