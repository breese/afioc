#include <cassert>
#include <vector>
#include <boost/bind.hpp>
#include <boost/afio/file_handle.hpp>

namespace boost
{
namespace afio
{

file_handle::file_handle(boost::asio::io_service& io)
    : pool(std::make_shared<std_thread_pool>(1)), // FIXME: Give io to pool
      io(pool->io_service()), // FIXME: Rename to get_io_service() (for Asio-compatibility)
      dispatcher(make_async_file_io_dispatcher(pool))
{
}

file_handle::~file_handle()
{
    // FIXME: Close file if open
    // FIXME: Check pending_operations
}

asio::io_service& file_handle::get_io_service()
{
    return io;
}

bool file_handle::is_open() const
{
    return (current_operation && current_operation->h); // FIXME: Also check future?
}

async_io_op file_handle::get_last_operation() const
{
    if (pending_operations.empty())
    {
        if (current_operation)
        {
            return *current_operation;
        }
        return async_io_op();
    }
    return *(pending_operations.back());
}

//-----------------------------------------------------------------------------
// Opening
//-----------------------------------------------------------------------------

void file_handle::async_open(const std::filesystem::path& path,
                             file_flags flags,
                             open_callback callback)
{
    // FIXME: Catch exceptions
    async_path_op_req request(std::filesystem::absolute(path), flags);
    std::shared_ptr<async_io_op> operation = std::make_shared<async_io_op>(dispatcher->file(request));
    auto rc = dispatcher->call(*operation,
                               boost::bind(&file_handle::process_open,
                                           this,
                                           operation,
                                           callback));
    pending_operations.push_back(std::make_shared<async_io_op>(rc.second));
}

void file_handle::process_open(std::shared_ptr<async_io_op> operation,
                               open_callback callback)
{
    if (!operation->h)
    {
        // FIXME: Pop pending operation?
        boost::system::error_code error(boost::system::errc::no_such_file_or_directory,
                                        boost::system::get_system_category());
        callback(error);
        return;
    }

    assert(operation->h->valid());
    try
    {
        std::shared_ptr<async_io_handle> handle = operation->h->get();
        boost::system::error_code success;
        pending_operations.pop_front(); // FIXME: Verify that element is correct
        current_operation = operation;
        callback(success);
    }
    catch (const boost::system::system_error& ex)
    {
        callback(ex.code());
    }
    catch (...)
    {
        boost::system::error_code error(boost::system::errc::bad_file_descriptor, // FIXME
                                        boost::system::get_system_category());
        callback(error);
    }
}

//-----------------------------------------------------------------------------
// Reading
//-----------------------------------------------------------------------------

void file_handle::async_read(boost::asio::mutable_buffer buffer,
                             read_callback callback)
{
    // Check that file is open
    if (!is_open())
    {
        boost::system::error_code error(boost::system::errc::bad_file_descriptor,
                                        boost::system::get_system_category());
        io.post(boost::bind(&file_handle::report_read,
                            this,
                            error,
                            0,
                            callback));
        return;
    }

    try
    {
        assert(current_operation->h->valid());
        auto beginning = boost::asio::buffer_cast<char *>(buffer);
        size_t size = boost::asio::buffer_size(buffer);
        size_t before_count = current_operation->h->get()->read_count();
        size_t remaining = current_operation->h->get()->lstat(metadata_flags::size).st_size - before_count;
        // FIXME: io.post eof if remaining == 0
        auto request = make_async_data_op_req(get_last_operation(),
                                              beginning,
                                              std::min(size, remaining),
                                              0);
        std::shared_ptr<async_io_op> operation = std::make_shared<async_io_op>(dispatcher->read(request));
        dispatcher->call(*operation,
                         boost::bind(&file_handle::process_read,
                                     this,
                                     operation,
                                     before_count,
                                     callback));
    }
    catch (const boost::system::system_error& ex)
    {
        io.post(boost::bind(&file_handle::report_read,
                            this,
                            ex.code(),
                            0,
                            callback));
    }
    catch (...)
    {
        boost::system::error_code error(boost::system::errc::bad_file_descriptor, // FIXME
                                        boost::system::get_system_category());
        io.post(boost::bind(&file_handle::report_read,
                            this,
                            error,
                            0,
                            callback));
    }
}

void file_handle::process_read(std::shared_ptr<async_io_op> operation,
                               size_t before_count,
                               read_callback callback)
{
    if (!operation->h)
    {
        boost::system::error_code error(boost::system::errc::bad_file_descriptor,
                                        boost::system::get_system_category());
        report_read(error, 0, callback);
        return;
    }

    assert(operation->h->valid());
    try
    {
        std::shared_ptr<async_io_handle> handle = operation->h->get();
        size_t bytes_transferred = handle->read_count() - before_count;
        // FIXME: Report bytes_transferred == 0 as eof?
        boost::system::error_code success;
        pending_operations.pop_front(); // FIXME: Verify that element is correct
        current_operation = operation;
        report_read(success, bytes_transferred, callback);
    }
    catch (const boost::system::system_error& ex)
    {
        report_read(ex.code(), 0, callback);
    }
    catch (...)
    {
        boost::system::error_code error(boost::system::errc::bad_file_descriptor, // FIXME
                                        boost::system::get_system_category());
        report_read(error, 0, callback);
    }
}

void file_handle::report_read(const boost::system::error_code& error,
                              std::size_t bytes_transferred,
                              read_callback callback)
{
    callback(error, bytes_transferred);
}

//-----------------------------------------------------------------------------
// Writing
//-----------------------------------------------------------------------------

void file_handle::async_write(boost::asio::const_buffer buffer,
                              write_callback callback)
{
    // FIXME: Check that file is opened for writing
    try
    {
        // FIXME: Extend file if necessary
        off_t offset = 0; // FIXME: Set when appending
        auto extend_request(dispatcher->truncate(get_last_operation(),
                                                 offset + boost::asio::buffer_size(buffer)));
        auto request = make_async_data_op_req(extend_request, buffer, offset);
        std::shared_ptr<async_io_op> operation = std::make_shared<async_io_op>(dispatcher->write(request));
        dispatcher->call(*operation,
                         boost::bind(&file_handle::process_write,
                                     this,
                                     operation,
                                     current_operation->h->get()->read_count(),
                                     callback));
    }
    catch (const boost::system::system_error& ex)
    {
        io.post(boost::bind(&file_handle::report_write,
                            this,
                            ex.code(),
                            0,
                            callback));
    }
    catch (...)
    {
        boost::system::error_code error(boost::system::errc::bad_file_descriptor, // FIXME
                                        boost::system::get_system_category());
        io.post(boost::bind(&file_handle::report_write,
                            this,
                            error,
                            0,
                            callback));
    }
}

void file_handle::process_write(std::shared_ptr<async_io_op> operation,
                                size_t before_count,
                                write_callback callback)
{
    if (!operation->h)
    {
        boost::system::error_code error(boost::system::errc::bad_file_descriptor,
                                        boost::system::get_system_category());
        report_read(error, 0, callback);
        return;
    }

    assert(operation->h->valid());
    try
    {
        std::shared_ptr<async_io_handle> handle = operation->h->get();
        size_t bytes_transferred = handle->write_count() - before_count;
        boost::system::error_code success;
        pending_operations.pop_front(); // FIXME: Verify that element is correct
        current_operation = operation;
        report_write(success, bytes_transferred, callback);
    }
    catch (const boost::system::system_error& ex)
    {
        report_write(ex.code(), 0, callback);
    }
    catch (...)
    {
        boost::system::error_code error(boost::system::errc::bad_file_descriptor, // FIXME
                                        boost::system::get_system_category());
        report_write(error, 0, callback);
    }
}

void file_handle::report_write(const boost::system::error_code& error,
                               std::size_t bytes_transferred,
                               read_callback callback)
{
    callback(error, bytes_transferred);
}

} // namespace afio
} // namespace boost
