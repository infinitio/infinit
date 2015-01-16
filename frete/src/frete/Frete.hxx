#ifndef FRETE_FRETE_HXX
# define FRETE_FRETE_HXX

ELLE_SERIALIZE_SIMPLE(frete::Frete::TransferInfo,
                      archive,
                      value,
                      format)
{
  enforce(format == 0);

  archive & value._count;
  archive & value._full_size;
  archive & value._files_info;
}


#endif
