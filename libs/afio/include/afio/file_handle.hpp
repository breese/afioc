#ifndef BOOST_AFIO_FILE_HANDLE_HPP
#define BOOST_AFIO_FILE_HANDLE_HPP

#include <deque>
#include <boost/function.hpp>
#include <boost/system/error_code.hpp>
#include <boost/asio/io_service.hpp>
#include <boost/asio/buffer.hpp>
#include <boost/afio/afio.hpp>

namespace boost
{
namespace afio
{

class file_handle
{
public:
    file_handle(boost::asio::io_service&);
    ~file_handle();

    asio::io_service& get_io_service();

    bool is_open() const;

    typedef boost::function<void (const boost::system::error_code&)> open_callback;
    void async_open(const std::filesystem::path&,
                    file_flags,
                    open_callback handler);

    typedef boost::function<void (const boost::system::error_code&, std::size_t)> read_callback;
    void async_read(boost::asio::mutable_buffer, read_callback);

    typedef boost::function<void (const boost::system::error_code&, std::size_t)> write_callback;
    void async_write(boost::asio::const_buffer, write_callback);


private:
    void process_open(std::shared_ptr<async_io_op>, open_callback);
    void process_read(std::shared_ptr<async_io_op>, size_t, read_callback);
    void report_read(const boost::system::error_code&, std::size_t, read_callback);
    void process_write(std::shared_ptr<async_io_op>, std::size_t, write_callback);
    void report_write(const boost::system::error_code&, std::size_t, read_callback);

    async_io_op get_last_operation() const;

private:
    class no_pool : public thread_source
    {
        // FIXME
    };

private:
    std::shared_ptr<thread_source> pool;
    boost::asio::io_service& io;
    std::shared_ptr<async_file_io_dispatcher_base> dispatcher;
    std::deque< std::shared_ptr<async_io_op> > pending_operations;
    std::shared_ptr<async_io_op> current_operation;
};

} // namespace afio
} // namespace boost

#endif // BOOST_AFIO_FILE_HANDLE_HPP
