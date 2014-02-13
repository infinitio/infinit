
$(document).ready(function() {
  
  // load transactions
  function loadTransactions(start, end, status) { 
    if (start == null) 
      start = moment().subtract('days', 7).startOf('day');
      
    if (end == null) 
      end = moment().subtract('days', 7);
      
    if (start == end)
      end.add('days', 1);
    
    url = '/metrics/transactions.html?start=' +start.toISOString() +'&end=' +end.toISOString();
    
    if (status)
      url += '&status=' +status
      
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
  
  loadTransactions();
  
  
  // Toggle transaction display
  setTimeout(function() {
    $(document).on("click", ".transaction", function() {
      $(this).children('.details').toggle();
      $(this).children('.summary').children('.date').toggle();
      $(this).toggleClass('extended');
    });
  }, 1000);
  
  // Cancel username clicks
  $(document).on("click", ".transaction h3 a", function() {
     event.preventDefault();
     console.info(this);
     
     $(this).qtip({
      overwrite: false,
      style: { classes: 'qtip-bootstrap' },
      content: $(this).parent().next('div'),
      show: { event: event.type, ready: true },
      hide: { event: 'unfocus' }
     }, event);
     
     return false;
  }); 
  
  // Toggle transactions display for a day 
  $(document).on("click", ".transactions h2 a", function() {
    $(this).parent().parent().find('.transaction').fadeToggle('fast');
  });
  
  
  // Datepicker
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
      loadTransactions(start, end, '');
    }
  );
  
  $('#datepicker').on('apply', function(ev, picker) {
    $('.transactions').empty().addClass('loading');
  });
  
  // Filter only some status
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
    loadTransactions(start, end, $(this).attr('title'));
  });
  
  // Change view
  $('.sidebar .views ul li a').click(function() {
    $('.sidebar .views ul li a').removeClass('active');
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

});











