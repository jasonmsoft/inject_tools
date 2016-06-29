-module(inject_server_sup).

-behaviour(supervisor).

%% API
-export([start_link/0]).

%% Supervisor callbacks
-export([init/1]).

%% Helper macro for declaring children of supervisor
-define(CHILD(I, Type), {I, {I, start_link, []}, permanent, 5000, Type, [I]}).

%% ===================================================================
%% API functions
%% ===================================================================

start_link() ->
    supervisor:start_link({local, ?MODULE}, ?MODULE, []).

%% ===================================================================
%% Supervisor callbacks
%% ===================================================================

init([]) ->

    {ok, { {one_for_one, 5, 10}, [?CHILD(inject_server, 'worker'),
                                    ?CHILD(client_mgr, 'worker'),
                                    ?CHILD(download_server, 'worker'),
                                    ?CHILD(content_server, 'worker'),
                                    ?CHILD(picture_proto_server, 'worker')]}}.

