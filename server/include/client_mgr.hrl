%%%-------------------------------------------------------------------
%%% @author Administrator
%%% @copyright (C) 2016, <COMPANY>
%%% @doc
%%%
%%% @end
%%% Created : 28. ÁùÔÂ 2016 15:43
%%%-------------------------------------------------------------------
-author("maji").

-record(client_info, {
    client_addr = 'undefined' ,  %%{ipaddress, port}
    client_state = 'undefined' ::atom(),
    client_sock = 'undefined' ::inet:socket(),
    client_info = 'undefined' ::term()
}).

-type client_info() ::client_info().