#include <iostream>
#include <boost/bind.hpp>
#include <boost/asio/placeholders.hpp>
#include <boost/afio/file_handle.hpp>

class file_copier
{
public:
    file_copier(boost::asio::io_service& io,
                std::filesystem::path source,
                std::filesystem::path target)
        : input(io),
          output(io),
          source(source),
          target(target),
          content(data, sizeof(data))
    {
        input.async_open(this->source,
                         boost::afio::file_flags::Read,
                         boost::bind(&file_copier::process_open_input,
                                     this,
                                     boost::asio::placeholders::error));
    }

    boost::asio::io_service& get_io_service()
    {
        return input.get_io_service();
    }

private:
    void start_read()
    {
        input.async_read(boost::asio::buffer(content),
                         boost::bind(&file_copier::process_read,
                                     this,
                                     boost::asio::placeholders::error,
                                     boost::asio::placeholders::bytes_transferred));
    }

    void process_open_input(const boost::system::error_code& error)
    {
        std::cout << __PRETTY_FUNCTION__ << ": " << error << std::endl;

        if (!error)
        {
            output.async_open(target,
                              boost::afio::file_flags::Create | boost::afio::file_flags::Write,
                              boost::bind(&file_copier::process_open_output,
                                          this,
                                          boost::asio::placeholders::error));
        }
    }

    void process_open_output(const boost::system::error_code& error)
    {
        std::cout << __PRETTY_FUNCTION__ << ": " << error << std::endl;

        if (!error)
        {
            start_read();
        }
    }

    void process_read(const boost::system::error_code& error,
                      std::size_t bytes_transferred)
    {
        std::cout << __PRETTY_FUNCTION__ << ": " << error << ", " << bytes_transferred << std::endl;
        if (!error)
        {
            output.async_write(boost::asio::buffer(content, bytes_transferred),
                               boost::bind(&file_copier::process_write,
                                           this,
                                           boost::asio::placeholders::error,
                                           boost::asio::placeholders::bytes_transferred));
        }
    }

    void process_write(const boost::system::error_code& error,
                       std::size_t bytes_transferred)
    {
        std::cout << __PRETTY_FUNCTION__ << ": " << error << ", " << bytes_transferred << std::endl;
    }

private:
    boost::afio::file_handle input;
    boost::afio::file_handle output;
    std::filesystem::path source;
    std::filesystem::path target;
    char data[1024]; // FIXME: Hardcoded to current size of "source.txt"
    boost::asio::mutable_buffer content;
};

int main(int argc, char *argv[])
{
    if (argc != 3)
    {
        std::cerr << "Usage: " << argv[0] << " <source> <target>" << std::endl;
        return 1;
    }
    boost::asio::io_service io;
    std::filesystem::path source(argv[1]);
    std::filesystem::path target(argv[2]);
    file_copier copier(io, source, target);
    copier.get_io_service().run(); // FIXME: Workaround (should be io.run())
    return 0;
}
