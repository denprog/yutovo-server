# Yutovo project
Yutovo is a powerful calculator with graphical representation of mathematics operations inside a text editor.

Yutovo server provides data for the [Yutovo web site](https://github.com/denprog/yutovo-web). It is based on [drogon](https://github.com/drogonframework/drogon/) and uses Postgresql database store user info and their documents.

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

```
mkdir -p build/debug
cd build/debug
cmake -DCMAKE_BUILD_TYPE=Debug ../..
make -sj && make install
```

Add yutovo-server.env file with your credentials in the ./yutovo-server folder:

```
DB_NAME=yutovo
DB_USER=yutovo
DB_PASSWORD=your_db_password
EMAIL_PASSWORD=your_email_password
```

Run the server:

```
env $(grep -v '^#' ../../yutovo-server.env | xargs) ./src/yutovo-serverd
```

Run the tests:

```
env $(grep -v '^#' ../../yutovo-server.env | xargs) ./test/yutovo-server_tests
```

Or one test:

```
env $(grep -v '^#' ../../yutovo-server.env | xargs) ./test/yutovo-server_tests --gtest_filter=AuthTest.register1
```
