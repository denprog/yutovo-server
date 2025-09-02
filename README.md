# Yutovo project
Yutovo is a powerful calculator with graphical representation of mathematics operations inside a text editor.

Yutovo server provides data and resources for the web site.

## Building for Ubuntu

Install [drogon](https://github.com/drogonframework/drogon/wiki/ENG-02-Installation), [jwt-cpp](https://github.com/Thalhammer/jwt-cpp).

Install the dependencies:

```
sudo update && sudo apt install -y build-essential cmake pkg-config libboost-iostreams1.83-dev libssl-dev libjsoncpp-dev uuid-dev zlib1g-dev libpq-dev libsqlite3-dev libbrotli-dev libgtest-dev libgmock-dev libjpeg-turbo8-dev cimg-dev
```

Set the variable:
```
export YUTOVO_DEPLOY=~/yutovo/deploy
```
Clone the project in the yutovo dir (select another branch if you want):

```
cd yutovo
git clone -b develop https://github.com/denprog/yutovo-server.git
```
Create the build directories and build the debug version:

Insert your passwords in CMakeLists.txt:
```
add_definitions(-DDB_PASSWORD="")
add_definitions(-DEMAIL_PASSWORD="")
```

```
mkdir -p build/debug
cd build/debug
cmake -DCMAKE_BUILD_TYPE=Debug ../..
make -sj && make install
```

Run the tests:

```
./test/yutovo-server_tests
```
