#ifndef SURFACE_GAP_STATE_OPERATION_HH
# define SURFACE_GAP_STATE_OPERATION_HH

# include "Operation.hh"

# include <elle/log.hh>
# include <elle/Exception.hh>

# include <functional>

namespace surface
{
  namespace gap
  {
    /// Base class for specialized operation managers.
    class OperationManager
    {
    private:
      // Glue to manage thread creation and destruction.
      template <typename T>
      struct OperationAdaptor;

    public:
      /// A process is indexed with a unique identifier.
      typedef size_t OperationId;

      /// The status of a process. failure or success implies that the process
      /// is terminated.
      enum class OperationStatus : int { failure = 0, success = 1, running = 2};

    private:
      typedef std::unique_ptr<Operation> OperationPtr;
      typedef std::unordered_map<OperationId, OperationPtr> OperationMap;
      OperationMap _operations;

    public:
      virtual
      ~OperationManager();

    public:
      /// Retreive the status of an existing operation or success if the id is 0.
      OperationStatus
      status(OperationId const id) const;

      /// @brief Remove a operation and throw the exception if any.
      ///
      /// @throw if the operation does not exist or if it is still running.
      void
      finalize(OperationId const id);

      /// @brief Cancel an operation if running.
      void
      cancel_operation(OperationId const id);

      /// Clean all terminated operations.
      void
      cleanup();

    protected:
     static
      OperationId
     _next_operation_id();

     /// Fire and store a new operation of type `Op` constructed with `args`.
     template <typename Op, typename... Args>
     OperationId
     _add(Args&&... args);

      virtual
      void
      _cancel(std::string const& name);

      virtual
      void
      _cancel_all(std::string const& name);

     virtual
     void
     _cancel_all();
    };

    /// Shortcut for lambda and bound functions.
    /// Accept `void()`, `void(Operation*)` and `void(Operation&)` prototypes.
    class LambdaOperation:
      public Operation
    {
    private:
      union
      {
        std::function<void()> _simple;
        std::function<void(Operation*)> _with_op;
        std::function<void(Operation&)> _with_op_ref;
      };
      enum
      {
        simple,
        with_op,
        with_op_ref,
      } _kind;

    public:
      LambdaOperation(std::string const& name,
                      std::function<void()> func):
        Operation{name},
        _simple{func},
        _kind{simple}
      {}

      LambdaOperation(std::string const& name,
                      std::function<void(Operation*)> func):
        Operation{name},
        _with_op{func},
        _kind{with_op}
      {}

      LambdaOperation(std::string const& name,
                      std::function<void(Operation&)> func):
        Operation{name},
        _with_op_ref{func},
        _kind{with_op_ref}
      {}

      LambdaOperation(LambdaOperation const&) = delete;
      LambdaOperation& operator =(LambdaOperation const&) = delete;

      ~LambdaOperation()
      {
        switch (this->_kind)
        {
        case simple:
          this->_simple.~function();
          break;
        case with_op:
          this->_with_op.~function();
          break;
        case with_op_ref:
          this->_with_op_ref.~function();
          break;
        }
      }

    protected:
      void _run() override
      {
        switch (this->_kind)
        {
        case simple:
          return this->_simple();
        case with_op:
          return this->_with_op(this);
        case with_op_ref:
          return this->_with_op_ref(*this);
        }
      }
    };
  }
}

# include "OperationManager.hxx"

#endif
