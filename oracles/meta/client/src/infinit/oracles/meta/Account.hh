#ifndef INFINIT_ORACLES_META_CLIENT_ACCOUNT_HH
# define INFINIT_ORACLES_META_CLIENT_ACCOUNT_HH

# include <elle/Printable.hh>
# include <elle/optional.hh>

# include <das/model.hh>

namespace infinit
{
  namespace oracles
  {
    namespace meta
    {
      typedef enum
        : unsigned int
      {
        AccountPlanType_Basic = 0,
        AccountPlanType_Plus,
        AccountPlanType_Premium,
        AccountPlanType_Team,
      } AccountPlanType;

      struct Account
        : public elle::Printable
      {
        struct ReferralActions
          : public elle::Printable
        {
          struct Referral
            : public elle::Printable
          {
            typedef enum
              : unsigned int
            {
              ReferralStatus_Pending = 0,
              ReferralStatus_Complete,
              ReferralStatus_Blocked,
            } ReferralStatus;

            typedef enum
              : unsigned int
            {
              ReferralMethod_Ghost = 0,
              ReferralMethod_Plain,
              ReferralMethod_Link,
            } ReferralMethod;

          /*-------------.
          | Construction |
          `-------------*/
          public:
            Referral(elle::serialization::SerializerIn& s);
            Referral();
            virtual
            ~Referral() = default;

          /*--------------.
          | Serialization |
          `--------------*/
          public:
            void
            serialize(elle::serialization::Serializer& s);

          /*-----------.
          | Attributes |
          `-----------*/
          public:
            bool has_logged_in;
            boost::optional<std::string> identifier;
            ReferralMethod method;
            ReferralStatus status;

          /*----------.
          | Printable |
          `----------*/
          protected:
            void
            print(std::ostream& stream) const override;
          };

        /*-------------.
        | Construction |
        `-------------*/
        public:
          ReferralActions(elle::serialization::SerializerIn& s);
          ReferralActions();
          virtual
          ~ReferralActions() = default;

        /*--------------.
        | Serialization |
        `--------------*/
        public:
          void
          serialize(elle::serialization::Serializer& s);

        /*-----------.
        | Attributes |
        `-----------*/
        public:
          bool has_avatar;
          uint32_t facebook_posts;
          uint32_t twitter_posts;
          std::vector<Referral> referrals;

        /*----------.
        | Printable |
        `----------*/
        protected:
          void
          print(std::ostream& stream) const override;
        };

        struct Quotas
          : public elle::Printable
        {
          struct QuotaUsage
            : public elle::Printable
          {
          /*-------------.
          | Construction |
          `-------------*/
          public:
            QuotaUsage(elle::serialization::SerializerIn& s);
            QuotaUsage();
            virtual
            ~QuotaUsage() = default;

          /*--------------.
          | Serialization |
          `--------------*/
          public:
            void
            serialize(elle::serialization::Serializer& s);

          /*-----------.
          | Attributes |
          `-----------*/
          public:
            boost::optional<uint64_t> quota;
            uint64_t used;

          /*----------.
          | Printable |
          `----------*/
          protected:
            void
            print(std::ostream& stream) const override;
          };

          struct Limit
            : public elle::Printable
          {
          /*-------------.
          | Construction |
          `-------------*/
          public:
            Limit(elle::serialization::SerializerIn& s);
            Limit() = default;
            virtual
            ~Limit() = default;

          /*--------------.
          | Serialization |
          `--------------*/
          public:
            void
            serialize(elle::serialization::Serializer& s);

          /*-----------.
          | Attributes |
          `-----------*/
          public:
            boost::optional<uint64_t> limit;

          /*----------.
          | Printable |
          `----------*/
          protected:
            void
            print(std::ostream& stream) const override;
          };

        /*-------------.
        | Construction |
        `-------------*/
        public:
          Quotas(elle::serialization::SerializerIn& s);
          Quotas() = default;
          virtual
          ~Quotas() = default;
        /*--------------.
        | Serialization |
        `--------------*/
        public:
          void
          serialize(elle::serialization::Serializer& s);

        /*-----------.
        | Attributes |
        `-----------*/
        public:
          QuotaUsage send_to_self;
          QuotaUsage links;
          Limit p2p;

        /*----------.
        | Printable |
        `----------*/
        protected:
          void
          print(std::ostream& stream) const override;
        };
      /*-------------.
      | Construction |
      `-------------*/
      public:
        Account() = default;
        Account(Account const& rhs);
        Account(elle::serialization::SerializerIn& s);
        das::Variable<std::string> custom_domain;
        das::Variable<std::string> link_format;
        das::Variable<AccountPlanType> plan;
        das::Variable<Quotas> quotas;
        das::Variable<ReferralActions> referral_actions;

        Account&
        operator =(Account const& rhs);

        typedef boost::signals2::signal<void (Account const&)> ChangedSignal;
        ELLE_ATTRIBUTE_RX(ChangedSignal, changed);

      /*--------------.
      | Serialization |
      `--------------*/
      public:
        void
        serialize(elle::serialization::Serializer& s);

      /*----------.
      | Printable |
      `----------*/
      protected:
        virtual
        void
        print(std::ostream& stream) const override;
      };

      std::ostream&
      operator <<(std::ostream& output, AccountPlanType account_type);
    }
  }
}

# include <infinit/oracles/meta/Account.hxx>

DAS_MODEL(
  infinit::oracles::meta::Account,
  (custom_domain, link_format, plan, quotas, referral_actions),
  DasAccount);
DAS_MODEL_DEFAULT(infinit::oracles::meta::Account, DasAccount);

#endif
