<?php

class MetaClient
{
  function __construct($meta_url)
  {
    $this->meta_url = $meta_url;
    $this->requests = new Requests_Session($meta_url);
  }

  function user($id)
  {
    $res = $this->requests->get('/users/' . $id, array(), array(), array());
    $json = json_decode($res->body, true);
    if (!$res->success)
      throw new Exception('unable to fetch user: ' . $json['reason']);
    return $json;
  }

  private $meta_url;
};

?>