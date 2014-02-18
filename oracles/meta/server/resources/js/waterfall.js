
$(document).ready(function() {
  
  // load transactions
  function loadTransactions(start, end, status, user, group) { 
    if (start == null) 
      start = moment().subtract('days', 8).startOf('day');
      
    if (end == null) 
      end = moment().subtract('days', 8);
      
    if (start == end)
      end.add('days', 1);
    
    url = '/metrics/transactions.html?start=' +start.toISOString() +'&end=' +end.toISOString();
    
    if (status)
      url += '&status=' +status
    
    if (user)
      url += '&user=' +user
    
    if (group)
      url += '&group=' +group
      
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
      url: '/metrics/transactions/groups'
    })
    .done(function(data) {
      var groups = data.groups;
      Object.keys(groups).forEach(function(key) {
        $('.sidebar .groups ul').append('<li><a class="group ' + groups[key].name + '" href="#group">' + groups[key].name + '</a> <a class="rm_group" href="#rm_group">x</a></li>');
      });
    })
    .error(function(data) {
      alert('Error getting group.');
    });
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
      $('#datepicker').html(start.format('MMMM D, YYYY') + ' &#8594; ' + end.format('MMMM D, YYYY'));
      loadTransactions(start = start, end = end);
    }
  );
  
  $('#datepicker').on('apply', function(ev, picker) {
    $('.transactions').empty().addClass('loading');
  });
  
  
  // Status filter
  $('.sidebar .status ul li a').click(function() {
    $('.sidebar .status ul li a').removeClass('active');
    $(this).addClass('active');
    
    var start_day = $('.daterangepicker .calendar.left table .start-date').text();
    var start_arr = $('.daterangepicker .calendar.left table th.month').text().split(' ');
    var start = moment(start_arr[0] + ' ' + start_day + ', ' + start_arr[1]);
    
    var end_day = $('.daterangepicker .calendar.right table .end-date').text();
    var end_arr = $('.daterangepicker .calendar.right table th.month').text().split(' ');
    var end = moment(end_arr[0] + ' ' + end_day + ', ' + end_arr[1]);
    
    $('.transactions').empty().addClass('loading');
    loadTransactions(start = start, end = end, status = $(this).attr('title'));
  });
  
  
  // User filter
  $(document).on("click", ".tooltip .show_user", function() {
    console.log($(this).attr('rel'));
    //$(this).parent('.tooltip').hide();
    loadTransactions(start = null, end = null, status = '', user = $(this).attr('rel'));
  });
  
  
  // Group filter
  $(document).on("click", ".sidebar .groups li a.group", function() {
    //loadTransactions(group = id);
  });
  
  
  // Create group 
  $('.sidebar .groups form').submit(function() { 
    $.ajax({
      type: 'PUT',
      url: '/metrics/transactions/groups/' + $('#input_add_group').val()
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
      url: '/metrics/transactions/groups/' + $(this).prev().text()
    })
    .done(function(data) {
      loadGroups();
    })
    .error(function(data) {
      alert('Error removing group.');
    });
    
    return false;
  });
  

});



















