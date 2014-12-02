#include <boost/lexical_cast.hpp>

#include <elle/json/json.hh>
#include <elle/log.hh>
#include <elle/format/base64.hh>
#include <elle/containers.hh>

#include <elle/container/vector.hh>

#include <elle/serialize/construct.hh>
#include <elle/serialize/extract.hh>
#include <elle/serialize/insert.hh>
#include <elle/serialize/PairSerializer.hxx>
#include <elle/serialize/VectorSerializer.hxx>

#include <aws/Exceptions.hh>

#include <surface/gap/S3TransferBufferer.hh>

ELLE_LOG_COMPONENT("surface.gap.S3TransferBufferer");

namespace surface
{
  namespace gap
  {
    /*-------------.
    | Construction |
    `-------------*/

    // Recipient.
    S3TransferBufferer::S3TransferBufferer(
      std::unique_ptr<aws::S3> s3,
      infinit::oracles::PeerTransaction& transaction,
      aws::S3::ErrorCallback on_error
      )
      : Super(transaction)
      , _count()
      , _full_size()
      , _files()
      , _key_code()
      , _raw_file(false)
      , _s3_handler(std::move(s3))
    {
      _s3_handler->on_error(on_error);
      try
      {
        // Fetch transfer meta-data from cloud.
        elle::Buffer buf = this->_s3_handler->get_object("meta_data");
        elle::InputStreamBuffer<elle::Buffer> buffer(buf);
        std::istream stream(&buffer);
        auto meta_data =
          boost::any_cast<elle::json::Object>(elle::json::read(stream));
        this->_count = boost::any_cast<int64_t>(meta_data["count"]);
        this->_full_size = boost::any_cast<int64_t>(meta_data["full_size"]);
        elle::serialize::from_string(elle::format::base64::decode(
          boost::any_cast<std::string>(
            meta_data["files"])).string()) >> this->_files;
        elle::serialize::from_string(elle::format::base64::decode(
          boost::any_cast<std::string>(
            meta_data["key_code"])).string()) >> this->_key_code;
      }
      catch (aws::FileNotFound const& e)
      {
        ELLE_LOG("%s: file not found for aws block meta-data", *this);
        throw DataExhausted();
      }
    }

    S3TransferBufferer::S3TransferBufferer(
      std::unique_ptr<aws::S3> s3,
      infinit::oracles::PeerTransaction& transaction,
      std::string const& file,
      aws::S3::ErrorCallback on_error)
      : Super(transaction)
      , _count()
      , _full_size()
      , _files()
      , _key_code()
      , _raw_file(true)
      , _s3_handler(std::move(s3))
    {
      _s3_handler->on_error(on_error);
      // That file constraint is mostly for validation, we could
      // just bool no_metadata=true and get done with it
      std::vector<std::pair<std::string, FileSize>> files
        = _s3_handler->list_remote_folder_full();
      if (files.size() != 1)
        ELLE_WARN("%s: Expected only one file in cloud folder, got %s", *this, files);
      auto match = elle::containers::find_pair_first(files, file);
      if (match == files.end())
        throw elle::Exception(elle::sprintf(
          "%s: Could not locate file in cloud data: expected '%s', got '%s'",
          *this, file, files));
      auto size = match->second;
      ELLE_DEBUG("%s: Advertising '%s' of size %s from cloud", *this, file, size);
      _files.push_back(std::make_pair(file, size));
      _count = 1;
      _full_size = size;
    }

    // Sender.
    S3TransferBufferer::S3TransferBufferer(
      std::unique_ptr<aws::S3> s3,
      infinit::oracles::PeerTransaction& transaction,
      aws::S3::ErrorCallback on_error,
      FileCount count,
      FileSize total_size,
      Files const& files,
      infinit::cryptography::Code const& key)
      : Super(transaction)
      , _count(count)
      , _full_size(total_size)
      , _files(files)
      , _key_code(key)
      , _s3_handler(std::move(s3))
    {
      _s3_handler->on_error(on_error);
      // Write transfer meta-data to cloud.
      // We binary serialize stuff, then base64-encode to be valid json
      // string, and then json-serialize
      // this is bad
      elle::json::Object meta_data;
      meta_data["count"] = this->_count;
      meta_data["full_size"] = this->_full_size;
      std::string files_str;
      elle::serialize::to_string(files_str) << this->_files;
      meta_data["files"] = elle::format::base64::encode(files_str).string();
      std::string key_str;
      elle::serialize::to_string(key_str) << this->_key_code;
      meta_data["key_code"] = elle::format::base64::encode(key_str).string();
      elle::Buffer buffer;
      elle::OutputStreamBuffer out_buffer(buffer);
      std::ostream stream(&out_buffer);
      elle::json::write(stream, meta_data);
      this->_s3_handler->put_object(buffer, "meta_data");
    }

    /*------.
    | Frete |
    `------*/
      std::vector<std::pair<std::string, S3TransferBufferer::FileSize>>
      S3TransferBufferer::files_info() const
      {
        return this->_files;
      }

      infinit::cryptography::Code
      S3TransferBufferer::read(FileID f, FileOffset start, FileSize size)
      {
        if (this->_raw_file)
          return infinit::cryptography::Code(get_chunk(f, start, size));
        else  // remote data is in whatever format it is, accept the call
          return infinit::cryptography::Code(this->get(f, start));
      }

      infinit::cryptography::Code
      S3TransferBufferer::encrypted_read(FileID f,
                                         FileOffset start,
                                         FileSize size)
      {
        return infinit::cryptography::Code(this->get(f, start));
      }

    /*----------.
    | Buffering |
    `----------*/

    void
    S3TransferBufferer::put(TransferBufferer::FileID file,
                            TransferBufferer::FileOffset offset,
                            TransferBufferer::FileSize size,
                            elle::ConstWeakBuffer const& b)
    {
      ELLE_DEBUG_SCOPE("%s: S3 put: %s (offset: %s, size: %s)",
                       *this, file, offset, size);
      std::string s3_name = this->_make_s3_name(file, offset);
      try
      {
        this->_s3_handler->put_object(b, s3_name);
      }
      catch (aws::AWSException const& e)
      {
        // XXX could retry.
        ELLE_ERR("%s: unable to put block: %s", *this, e.what());
        throw;
      }
    }

    elle::Buffer
    S3TransferBufferer::get_chunk(TransferBufferer::FileID file,
                                    TransferBufferer::FileSize offset,
                                    TransferBufferer::FileSize size)
    {
      ELLE_DEBUG_SCOPE("%s: S3 get: %s (offset: %s, size: %s)",
                       *this, file, offset, size);
      try
      {
        elle::Buffer res;
        res = this->_s3_handler->get_object_chunk(_files.at(file).first, offset, size);
        ELLE_ASSERT_GTE(size, res.size());
        return res;
      }
      catch (aws::FileNotFound const& e)
      {
        ELLE_LOG("%s: file not found on aws for chunk %s/%s",
                 *this, file, offset);
        throw DataExhausted();
      }
    }

    elle::Buffer
    S3TransferBufferer::get(TransferBufferer::FileID file,
                            TransferBufferer::FileSize offset)
    {
      ELLE_DEBUG_SCOPE("%s: S3 get: %s (offset: %s)", *this, file, offset);
      std::string s3_name = this->_make_s3_name(file, offset);
      try
      {
        elle::Buffer res;
        res = this->_s3_handler->get_object(s3_name);
        return res;
        // XXX should clean up folder once transaction has been completed.
      }
      catch (aws::FileNotFound const& e)
      {
        ELLE_LOG("%s: file not found on aws for block %s/%s",
                 *this, file, offset);
        throw DataExhausted();
      }
      catch (aws::CorruptedData const& e)
      {
        throw;
      }
      catch (aws::AWSException const& e)
      {
        // XXX could retry.
        ELLE_ERR("%s: unable to get block: %s", *this, e.what());
        // FIXME: differenciate AWS other exception and "data not here"
        throw;
      }
    }

    TransferBufferer::List
    S3TransferBufferer::list()
    {
      ELLE_DEBUG_SCOPE("%s: S3 list", *this);
      try
      {
        TransferBufferer::List res;
        aws::S3::List list;
        std::string marker = "";
        bool first = true;
        do
        {
          list = this->_s3_handler->list_remote_folder(marker);
          marker = list.back().first;
          // If we're running a second+ time, it means that we'll get marker
          // element twice, so remove it.
          if (!first)
          {
            list.erase(list.begin());
          }
          else
          {
            first = false;
          }
          TransferBufferer::List converted_list = this->_convert_list(list);
          res.insert(res.end(), converted_list.begin(), converted_list.end());
        }
        while (list.size() >= 1000);
        return res;
      }
      catch (aws::AWSException const& e)
      {
        // XXX could retry.
        ELLE_ERR("%s: unable to get block: %s", *this, e.what());
        throw;
      }
    }

    /*--------.
    | Helpers |
    `--------*/
    TransferBufferer::FileOffset
    S3TransferBufferer::_offset_from_s3_name(std::string const& s3_name)
    {
      int pos = s3_name.find("_");
      std::string res = s3_name.substr(pos + 1, s3_name.size() - (pos + 1));
      return boost::lexical_cast<TransferBufferer::FileOffset>(res);
    }

    TransferBufferer::FileID
    S3TransferBufferer::_file_id_from_s3_name(std::string const& s3_name)
    {
      int pos = s3_name.find("_");
      std::string res = s3_name.substr(0, pos);
      return boost::lexical_cast<TransferBufferer::FileID>(res);
    }

    std::string
    S3TransferBufferer::_make_s3_name(
      TransferBufferer::FileID const& file,
      TransferBufferer::FileOffset const& offset)
    {
      std::string suffix = std::to_string(offset);
      std::string total_size_str =
        std::to_string(this->transaction().total_size);
      ELLE_ASSERT(total_size_str.size() >= suffix.size());
      suffix.insert(0, total_size_str.size() - suffix.size(), '0');
      return elle::sprintf("%012s_%s", file, suffix);
    }

    TransferBufferer::List
    S3TransferBufferer::_convert_list(aws::S3::List const& list)
    {
      TransferBufferer::List res;
      for (auto const& item: list)
      {
        std::pair<FileOffset, FileSize> inner;
        inner = std::make_pair(this->_offset_from_s3_name(item.first),
                               boost::lexical_cast<FileSize>(item.second));
        std::pair<TransferBufferer::FileID,
                  std::pair<TransferBufferer::FileOffset,
                            TransferBufferer::FileSize>> outter;
        outter = std::make_pair(this->_file_id_from_s3_name(item.first), inner);
        res.push_back(outter);
      }
      return res;
    }

    void
    S3TransferBufferer::cleanup()
    {
      // Consider cleanup errors as nonfatal for the user
      try
      {
        this->_s3_handler->delete_folder();
      }
      catch (const reactor::Terminate&)
      {
        ELLE_WARN("%s: terminated while cleaning up", *this);
        throw;
      }
      catch (...)
      {
        ELLE_WARN("%s: losing cleanup exception %s", *this,
                  elle::exception_string());
      }
    }

    /*----------.
    | Printable |
    `----------*/

    void
    S3TransferBufferer::print(std::ostream& stream) const
    {
      stream << "S3TransferBufferer (transaction_id: " << this->transaction().id
             << ")";
    }
  }
}
