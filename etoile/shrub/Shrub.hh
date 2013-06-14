#ifndef ETOILE_SHRUB_SHRUB_HH
# define ETOILE_SHRUB_SHRUB_HH

# include <boost/date_time/posix_time/posix_time.hpp>

# include <reactor/fwd.hh>

# include <elle/attribute.hh>
# include <elle/container/timeline/Timeline.hh>
# include <elle/types.hh>
# include <elle/utility/Duration.hh>

# include <etoile/path/fwd.hh>
# include <etoile/shrub/Riffle.hh>
# include <etoile/shrub/fwd.hh>

namespace etoile
{
  /// This namespace contains everything related to the shrub i.e the
  /// path-specific cache.
  namespace shrub
  {
    extern Shrub* global_shrub;

    /// The shrub i.e path cache relies on the LRU algorithm by keeping two data
    /// structures: a map for looking up and a queue for removing the least
    /// recently used riffles quickly.
    ///
    /// To avoid using too much memory for these data structures, both point to
    /// the same data: riffles.
    ///
    /// Noteworthy is that, since this cache is used for paths and that paths
    /// follow a pattern where /music/meshuggah is a subset of /music, the data
    /// structure for storing the paths is a tree.  Indeed, the Riffle class
    /// keeps a name such as 'meshuggah' along with a map of all the child
    /// entries.
    ///
    /// This design has been chosen to speed up the resolution process. Indeed,
    /// this cache is used when a path must be resolved into a venue. the
    /// objective of the cache is thus to find the longest part of a given path.
    ///
    /// For example, given /music/meshuggah/nothing/, the objective is to find
    /// the corresponding address of this directory object. instead of trying
    /// /music/meshuggah/nothing/, then /music/meshuggah/, then /music/ etc. the
    /// designed cache is capable of returning the longest match within a single
    /// pass because riffles are hierarchically organised.
    ///
    /// Note that several parameters can be configured through the configuration
    /// file:
    ///
    ///   o status: indicates whether the shrub should be used for
    ///             caching paths.
    ///   o capacity: indicates the number of riffles the shrub can
    ///               maintain before rejecting additional entries.
    ///   o frequency: indicates, in milliseconds, how often the sweeper
    ///                should be triggered in order to evict expired riffles.
    ///                note that the frequency is expressed in milliseconds.
    ///   o lifespan: indicates the riffles' lifespan before being considered
    ///               as having expired. note that every update on a riffle
    ///               resets the "expiration timeout", so to speak. note
    ///               that the lifespan is expressed in seconds, not
    ///               milliseconds.
    ///
    class Shrub
    {
    /*-------------.
    | Construction |
    `-------------*/
    public:
      /// Create a Shrub.
      Shrub(elle::Size capacity,
            boost::posix_time::time_duration const& lifespan,
            boost::posix_time::time_duration const& sweep_frequency);
      /// Destroy a Shrub.
      ~Shrub();

    /*--------------.
    | Configuration |
    `--------------*/
      ELLE_ATTRIBUTE_R(elle::Size, capacity);
      ELLE_ATTRIBUTE_R(boost::posix_time::time_duration, lifespan);
      ELLE_ATTRIBUTE_R(boost::posix_time::time_duration, sweep_frequency);

    public:
      /// Allocate \param size slots for the introduction of new riffles.
      void
      allocate(const elle::Natural32 size);
      /// Fill the \param venue by resolving the given \param route as much as
      /// possible.
      void
      resolve(path::Route const& route,
              path::Venue& venue);
      /// Fill the shrub with the given \param venue for  \param route.
      void
      update(const path::Route& route,
             const path::Venue& venue);
      /// Remove the \param route from the Shrub.
      void
      evict(const path::Route& route);
      /// Dump the whole shrub.
      void
      show(const elle::Natural32 = 0);
      /// Remove all routes from the Shrub.
      void
      clear();

    /*------.
    | Cache |
    `------*/
    private:
      /// Remove expired routes.
      void
      _sweep();
      /// The thread running _sweep regularly.
      ELLE_ATTRIBUTE(reactor::Thread*, sweeper);
      friend class Riffle;
      ELLE_ATTRIBUTE(Riffle*, riffles);
      ELLE_ATTRIBUTE(elle::container::timeline::Timeline<Riffle*>, queue);
    };

  }
}

#endif
