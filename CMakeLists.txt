cmake_minimum_required(VERSION 3.5)
project(Chat)

#
set(LIBRARY_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/lib)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -std=c++11 -fPIC")
enable_testing()

set(CHAT_SOURCES
    Acceptor.cc
    Buffer.cc
    Channel.cc
    CurrentThread.cc
    EPollPoller.cc
    EventLoop.cc
    EventLoopThread.cc
    EventLoopThreadPool.cc
    InetAddress.cc
    Logger.cc
    Poller.cc
    Socket.cc
    TcpConnection.cc
    TcpServer.cc
    Thread.cc
    TimeStamp.cc
    newDefaultPoller.cc
)

add_library(chat SHARED ${CHAT_SOURCES})

target_include_directories(chat PUBLIC ${PROJECT_SOURCE_DIR})

if(EXISTS ${PROJECT_SOURCE_DIR}/examples/test_eventloop.cc)
    add_executable(test_eventloop examples/test_eventloop.cc)
    target_link_libraries(test_eventloop chat pthread)
    target_include_directories(test_eventloop PRIVATE ${PROJECT_SOURCE_DIR})
endif()

if(EXISTS ${PROJECT_SOURCE_DIR}/examples/test_runinloop.cc)
    add_executable(test_runinloop examples/test_runinloop.cc)
    target_link_libraries(test_runinloop chat pthread)
    target_include_directories(test_runinloop PRIVATE ${PROJECT_SOURCE_DIR})
endif()

add_executable(echo_server examples/echo_server.cc)
target_link_libraries(echo_server chat pthread)
target_include_directories(echo_server PRIVATE ${PROJECT_SOURCE_DIR})

add_executable(echo_server_bench bench/echo_server_bench.cc)
target_link_libraries(echo_server_bench chat pthread)
target_include_directories(echo_server_bench PRIVATE ${PROJECT_SOURCE_DIR})

add_executable(long_conn_client bench/long_conn_client.cc)
target_link_libraries(long_conn_client pthread)
target_include_directories(long_conn_client PRIVATE ${PROJECT_SOURCE_DIR})

add_executable(socket_backlog_test tests/socket_backlog_test.cc)
target_link_libraries(socket_backlog_test chat pthread)
target_include_directories(socket_backlog_test PRIVATE ${PROJECT_SOURCE_DIR})
add_test(NAME socket_backlog_test COMMAND socket_backlog_test)

add_test(
    NAME benchmark_cli_smoke
    COMMAND bash ${PROJECT_SOURCE_DIR}/tests/benchmark_cli_smoke.sh ${CMAKE_BINARY_DIR}
)

add_test(
    NAME pressure_report_smoke
    COMMAND bash ${PROJECT_SOURCE_DIR}/tests/pressure_report_smoke.sh ${PROJECT_SOURCE_DIR}
)
