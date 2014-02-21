$(document).ready(function() {

  /************************************************
  /* All pages
  /************************************************/

  $.urlParam = function(name){
    var results = new RegExp('[\\?&amp;]' + name + '=([^&amp;#]*)').exec(window.location.href);
    return results[1] || 0;
  }

  function getURLParameter(name)
  {
    var param = RegExp(name + '=' + '(.+?)(&|$)').exec(location.search)
    if (param)
      return decodeURI(param[1])
    else
      return null
  }

  // Tooltips
  $(document).on("click", ".show_tooltip", function() {

    var that = this;
    var select = $(that).parent().next('div').find('.add_group');
    event.preventDefault();

    // Show tooltip
    $(this).qtip({
      overwrite: false,
      style: { classes: 'qtip-bootstrap' },
      content: $(this).parent().next('div'),
      show: { event: event.type, ready: true },
      hide: { event: 'unfocus' }
    }, event);

    // Populate Add To Group select
    $.ajax({
     type: 'GET',
     url: '/waterfall/transactions/groups/'
    })
    .done(function(data) {
     var groups = data.groups;

     Object.keys(groups).forEach(function(key) {
       select.append('<option value="' + groups[key].name + '">' + groups[key].name + '</option>');
     });
    })
    .error(function(data) {
      alert('Error getting groups.');
    });

    // Add To Group
    select.change(function() {
      var group = $(this).find('option:selected').text();
      var user_id = $(this).attr('rel');

      $.ajax({
        type: 'PUT',
        url: '/waterfall/transactions/groups/' + group + '/' + user_id
      })
      .done(function(data) {
        $(that).qtip('hide');
        loadGroupsHtml();
      })
      .error(function(data) {
        alert('Error adding to group.');
      });
    });

     return false;
  });


  // Search user
  $('.sidebar .search form').submit(function() {
    var text = $('#search_user input').val();

    $.ajax({
      type: 'GET',
      url: '/waterfall/users.html' + '?search=' + text
    })
    .done(function(data) {
      $('#search-results').html(data);
    })
    .error(function(data) {
      alert('Error searching user.');
    });

    return false;
  });



  /************************************************
  /* Waterfall page
  /************************************************/

  if ($('body').hasClass('waterfall')) {

    // load transactions
    function loadTransactions(start, end, status, users, groups) {
      if (start == null && users == null)
        start = moment().subtract('days', 1).startOf('day');

      if (end == null && users == null)
        end = moment();

      if (start == end && users == null)
        end.add('days', 1);

      // default: display last year of a single user transaction
      if (start == null && users != null) {
        start = moment().subtract('year', 1).startOf('day');
        end = moment();
      }

      if (!users)
      {
        users = getURLParameter('users');
        if (users)
        {
          users = getURLParameter('users');
          start = moment().subtract('year', 1).startOf('day');
          end = moment();
        }
      } 
      else 
      {
        users = JSON.stringify(users);  
      }

      url = '/waterfall/transactions.html?';

      if (start)
        url += '&start=' + start.toISOString() + '&end=' + end.toISOString()

      if (status)
        url += '&status=' + status

      if (users)
        url += '&users=' + users

      if (groups)
        url += '&groups=' + JSON.stringify(groups)

      $.ajax({
        url: url
      })
      .done(function(data) {
        $('.transactions').removeClass('loading').html(data);
      })
      .error(function(data) {
        $('.transactions').removeClass('loading').text('Error while loading transactions.');
      });
    }

    // load groups
    function loadGroups() {
      $('.sidebar .groups .group').parent().remove();

      $.ajax({
        type: 'GET',
        url: '/waterfall/transactions/groups'
      })
      .done(function(data) {
        var groups = data.groups;
        Object.keys(groups).forEach(function(key) {
          $('.sidebar .groups ul').append('<li><a class="filter_group" href="#group">' + groups[key].name + '</a></li>');
        });
      })
      .error(function(data) {
        alert('Error getting group.');
      });
    }

    function getCurrentStartDate() {
      var start_day = $('.daterangepicker .calendar.left table .start-date').text();
      var start_arr = $('.daterangepicker .calendar.left table th.month').text().split(' ');
      return moment(start_arr[0] + ' ' + start_day + ', ' + start_arr[1]);
    }

    function getCurrentEndDate() {
      var end_day = $('.daterangepicker .calendar.right table .end-date').text();
      var end_arr = $('.daterangepicker .calendar.right table th.month').text().split(' ');
      return moment(end_arr[0] + ' ' + end_day + ', ' + end_arr[1]);
    }

    function getSelectedStatus() {
      return $('.sidebar .status ul li a.active').attr('title');
    }

    function getSelectedGroups() {
      var groups = Array();
      $('.filter_group.active').each(function() {
        groups.push($(this).text());
      });

      return groups;
    }

    function resetFilters() {
      $('.filter_group').removeClass('.active');
      $('.sidebar .status ul li a').removeClass('active');
    }

    loadTransactions();
    loadGroups();


    // Toggle transaction display
    setTimeout(function() {
      $(document).on("click", ".transaction", function() {
        $(this).children('.details').toggle();
        $(this).children('.summary').children('.date').toggle();
        $(this).toggleClass('extended');
      });
    }, 1000);


    // Toggle transactions display for a day
    $(document).on("click", ".transactions h2 a", function() {
      $(this).parent().parent().find('.transaction').fadeToggle('fast');
    });


    // Change view
    $('.views ul li a').click(function() {
      $('.views ul li a').removeClass('active');
      $(this).addClass('active');

      var view = $(this).attr('title');

      if (view == 'by_day') {
        $('.transactions').removeClass('by_transaction').addClass($(this).attr('title'));
        $('.transaction').fadeOut('fast');

      } else if (view == 'by_transaction') {
        $('.transactions').removeClass('by_day').addClass($(this).attr('title'));
        $('.transaction').fadeIn('fast');
      }
    });


    // Date filter
    $('#datepicker').daterangepicker({
      ranges: {
       'Today': [moment(), moment()],
       'Yesterday': [moment().subtract('days', 1), moment().subtract('days', 1)],
       'Last 7 Days': [moment().subtract('days', 6), moment()],
       'Last 30 Days': [moment().subtract('days', 29), moment()],
       'This Month': [moment().startOf('month'), moment().endOf('month')],
       'Last Month': [moment().subtract('month', 1).startOf('month'), moment().subtract('month', 1).endOf('month')]
      },
        startDate: moment().startOf('day'),
        endDate: moment()
      },
      function(start, end) {
        $('.transactions').empty().addClass('loading');
        
        $('#datepicker').html(start.format('MMMM D, YYYY') + ' &#8594; ' + end.format('MMMM D, YYYY'));
        var groups = getSelectedGroups();
        var status = getSelectedStatus();
        
        loadTransactions(start = start, end = end, status = status, user = null, groups = groups);
      }
    );


    // Status filter
    $('.sidebar .status ul li a').click(function() {
      $('.sidebar .status ul li a').removeClass('active');
      $(this).addClass('active');

      var start = getCurrentStartDate();
      var end = getCurrentEndDate();
      var groups = getSelectedGroups();

      $('.transactions').empty().addClass('loading');
      loadTransactions(start = start, end = end, status = $(this).attr('title'), user = null, groups = groups);
    });


    // User filter
    $(document).on("click", ".show_user", function() {
      $(this).parent('.tooltip').qtip('hide');
      resetFilters();

      var users = Array();
      users.push($(this).attr('rel'));

      loadTransactions(start = null, end = null, status = '', users = users);
    });


    // Group filter
    $(document).on("click", ".sidebar .groups .filter_group", function() {
      $(this).toggleClass('active');

      var start = getCurrentStartDate();
      var end = getCurrentEndDate();
      var status = getSelectedStatus();
      var groups = getSelectedGroups();

      loadTransactions(start = start, end = end, status = status, user = null, groups = groups);
    });


    // Create group
    $('.sidebar .groups form').submit(function() {
      $.ajax({
        type: 'PUT',
        url: '/waterfall/transactions/groups/' + $('#input_add_group').val()
      })
      .done(function(data) {
        $(".sidebar .groups input").val('');
        loadGroups();
      })
      .error(function(data) {
        alert('Error creating group.');
      });

      return false;
    });

    // Remove group
    $(document).on("click", ".sidebar .groups .rm_group", function() {
      $.ajax({
        type: 'DELETE',
        url: '/waterfall/transactions/groups/' + $(this).prev().text()
      })
      .done(function(data) {
        loadGroups();
      })
      .error(function(data) {
        alert('Error removing group.');
      });

      return false;
    });
  }



  /************************************************
  /* Groups page
  /************************************************/

  if ($('body').hasClass('groups')) {


    // load groups
    function loadGroupsHtml() {
      $('.sidebar .groups .group').parent().remove();

      $.ajax({
        type: 'GET',
        url: '/waterfall/groups.html'
      })
      .done(function(data) {
        $('.groups-list').html(data);
      })
      .error(function(data) {
        alert('Error getting groups.');
      });
    }

    loadGroupsHtml();

    // Create group
    $('#add_group').submit(function() {
      $.ajax({
        type: 'PUT',
        url: '/waterfall/transactions/groups/' + $('#add_group input').val()
      })
      .done(function(data) {
        $('#add_group input').val('');
        loadGroupsHtml();
      })
      .error(function(data) {
        alert('Error creating group.');
      });

      return false;
    });

    // Remove group
    $(document).on("click", ".rm_group", function() {
      $.ajax({
        type: 'DELETE',
        url: '/waterfall/transactions/groups/' + $(this).prev().prev().text()
      })
      .done(function(data) {
        loadGroupsHtml();
      })
      .error(function(data) {
        alert('Error removing group.');
      });

      return false;
    });

    // Remove from group
    $(document).on("click", ".rm_from_group", function() {
      var group = $(this).parent().parent().parent().attr('rel');
      var user_id = $(this).prev().attr('rel');
      $.ajax({
        type: 'DELETE',
        url: '/waterfall/transactions/groups/' + group + '/' + user_id
      })
      .done(function(data) {
        loadGroupsHtml();
      })
      .error(function(data) {
        alert('Error removing from group.');
      });
    });

  }

});
