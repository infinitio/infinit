$(document).ready(function() {
  
  // load groups
  function loadGroups() {
    $('.sidebar .groups .group').parent().remove();
    
    $.ajax({
      type: 'GET',
      url: '/metrics/groups.html'
    })
    .done(function(data) {
      $('.groups-list').html(data);
    })
    .error(function(data) {
      alert('Error getting groups.');
    });
  }
  
  loadGroups();
  
  // Create group 
  $('#add_group').submit(function() { 
  
    $.ajax({
      type: 'PUT',
      url: '/metrics/transactions/groups/' + $('#add_group input').val()
    })
    .done(function(data) { 
      $('#add_group input').val('');
      loadGroups();
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