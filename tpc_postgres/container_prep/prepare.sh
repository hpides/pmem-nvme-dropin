
CMAKE_FROM_SOURCE=true

while getopts ':a' 'OPTKEY'; do
    case ${OPTKEY} in
        'a')
            # Update the value of the option x flag we defined above
            CMAKE_FROM_SOURCE=false
            ;;
        '?')
            echo "INVALID OPTION -- ${OPTARG}" >&2
            exit 1
            ;;
        ':')
            echo "MISSING ARGUMENT for option -- ${OPTARG}" >&2
            exit 1
            ;;
        *)
            echo "UNIMPLEMENTED OPTION -- ${OPTKEY}" >&2
            exit 1
            ;;
    esac
done

if ${CMAKE_FROM_SOURCE}; then
    echo "I will install Cmake from source"
else
    echo "I will install Cmake using apt"
fi

apt -y update
apt -y upgrade
apt -y install build-essential manpages-dev git wget libssl-dev software-properties-common
add-apt-repository ppa:ubuntu-toolchain-r/test -y
add-apt-repository ppa:neovim-ppa/stable -y
apt-get -y update
apt-get -y install gcc g++ neovim
apt-get -y install libnuma-dev libndctl-dev numactl


if ${CMAKE_FROM_SOURCE}; then
    mkdir cmake
    cd cmake
    version=3.19
    build=1
    mkdir ~/temp
    cd ~/temp
    wget https://cmake.org/files/v$version/cmake-$version.$build.tar.gz
    tar -xzvf cmake-$version.$build.tar.gz
    cd cmake-$version.$build/
    ./bootstrap
    make -j$(nproc)
    make install
else
    apt-get -y install cmake
fi
