#include <surface/gap/S3TransferBufferer.hh>

#include <boost/lexical_cast.hpp>

#include <elle/log.hh>

#include <aws/Exceptions.hh>

ELLE_LOG_COMPONENT("surface.gap.S3TransferBufferer");

namespace surface
{
  namespace gap
  {
    /*-------------.
    | Construction |
    `-------------*/

    S3TransferBufferer::S3TransferBufferer(
      infinit::oracles::Transaction& transaction,
      aws::Credentials const& credentials,
      std::string const& bucket_name):
        Super(transaction),
        _bucket_name(bucket_name),
        _credentials(credentials),
        _remote_folder(this->transaction().id),
        _s3_handler(this->_bucket_name, this->_remote_folder,
                    this->_credentials)
    {}

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
        this->_s3_handler.put_object(b, s3_name);
      }
      catch (aws::AWSException const& e)
      {
        // XXX could retry.
        ELLE_ERR("%s: unable to put block: %s", *this, e.error());
        throw;
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
        res = this->_s3_handler.get_object(s3_name);
        this->_s3_handler.delete_object(s3_name);
        return res;
        // XXX should clean up folder once transaction has been completed.
      }
      catch (aws::AWSException const& e)
      {
        // XXX could retry.
        ELLE_ERR("%s: unable to get block: %s", *this, e.error());
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
          list = this->_s3_handler.list_remote_folder(marker);
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
        ELLE_ERR("%s: unable to get block: %s", *this, e.error());
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
      std::string prefix = boost::lexical_cast<std::string>(file);
      std::string no_files_str =
        boost::lexical_cast<std::string>(this->transaction().files_count);
      prefix.insert(0, no_files_str.size() - prefix.size(), '0');

      std::string suffix = boost::lexical_cast<std::string>(offset);
      std::string total_size_str =
        boost::lexical_cast<std::string>(this->transaction().total_size);
      suffix.insert(0, total_size_str.size() - suffix.size(), '0');

      return elle::sprintf("%s_%s", prefix, suffix);
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
