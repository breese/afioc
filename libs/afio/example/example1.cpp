#include <boost/afio/afio.hpp>

int main(int argc, char *argv[])
{
    if (argc != 2)
        return 1;

    using namespace boost::afio;
    auto dispatcher = make_async_file_io_dispatcher();
    char input[1024];
    auto opened_file = dispatcher->file(async_path_op_req(argv[1]));
    auto read_file = dispatcher->read(make_async_data_op_req(opened_file, input, 0));
    when_all(read_file).wait();
    return 0;
}
