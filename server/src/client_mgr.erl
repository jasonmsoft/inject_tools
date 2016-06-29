%%%-------------------------------------------------------------------
%%% @author Administrator
%%% @copyright (C) 2016, <COMPANY>
%%% @doc
%%%
%%% @end
%%% Created : 28. ÁùÔÂ 2016 14:36
%%%-------------------------------------------------------------------
-module(client_mgr).
-author("Administrator").

-behaviour(gen_server).

%% API
-export([start_link/0]).

%% gen_server callbacks
-export([init/1,
    handle_call/3,
    handle_cast/2,
    handle_info/2,
    terminate/2,
    code_change/3]).

-export([find_client/2, add_client/1, del_client/2, update_client/3, update_client_state/3]).


-define(SERVER, ?MODULE).

-record(state, {table_id = 'undefined'}).

-include_lib("../include/client_mgr.hrl").


%%%===================================================================
%%% API
%%%===================================================================
-spec(find_client(ClientAddr ::inet:ip_address(), ClientPort ::integer()) -> {ok, Clients ::client_info()} | {error, R ::binary()}).
find_client(ClientAddr, ClientPort) ->
    gen_server:call(?MODULE, {'find_client', ClientAddr, ClientPort}).


-spec(add_client(Client ::client_info()) -> ok).
add_client(Client) ->
    gen_server:cast(?MODULE, {'add_client', Client}).

-spec(del_client(ClientAddr ::inet:ip_address(), ClientPort ::integer()) ->ok).
del_client(ClientAddr , ClientPort) ->
    gen_server:cast(?MODULE, {'del_client',ClientAddr, ClientPort}).

update_client(ClientAddr, ClientPort, Info) ->
    gen_server:cast(?MODULE, {'update_client',ClientAddr, ClientPort, Info}).

update_client_state(ClientAddr, ClientPort, NewState) ->
    gen_server:cast(?MODULE, {'update_client_state', ClientAddr, ClientPort, NewState}).


%%--------------------------------------------------------------------
%% @doc
%% Starts the server
%%
%% @end
%%--------------------------------------------------------------------
-spec(start_link() ->
    {ok, Pid :: pid()} | ignore | {error, Reason :: term()}).
start_link() ->
    gen_server:start_link({local, ?SERVER}, ?MODULE, [], []).

%%%===================================================================
%%% gen_server callbacks
%%%===================================================================

%%--------------------------------------------------------------------
%% @private
%% @doc
%% Initializes the server
%%
%% @spec init(Args) -> {ok, State} |
%%                     {ok, State, Timeout} |
%%                     ignore |
%%                     {stop, Reason}
%% @end
%%--------------------------------------------------------------------
-spec(init(Args :: term()) ->
    {ok, State :: #state{}} | {ok, State :: #state{}, timeout() | hibernate} |
    {stop, Reason :: term()} | ignore).
init([]) ->
    self() ! 'init',
    {ok, #state{}}.

%%--------------------------------------------------------------------
%% @private
%% @doc
%% Handling call messages
%%
%% @end
%%--------------------------------------------------------------------
-spec(handle_call(Request :: term(), From :: {pid(), Tag :: term()},
    State :: #state{}) ->
    {reply, Reply :: term(), NewState :: #state{}} |
    {reply, Reply :: term(), NewState :: #state{}, timeout() | hibernate} |
    {noreply, NewState :: #state{}} |
    {noreply, NewState :: #state{}, timeout() | hibernate} |
    {stop, Reason :: term(), Reply :: term(), NewState :: #state{}} |
    {stop, Reason :: term(), NewState :: #state{}}).

handle_call({'find_client', ClientAddr, ClientPort}, _From, State) ->
    case ets:lookup(client,{ClientAddr, ClientPort}) of
        [] ->{reply, {error, <<"not found">>}, State};
        Clients ->{reply, {ok, Clients}, State}
    end;

handle_call(_Request, _From, State) ->
    {reply, ok, State}.

%%--------------------------------------------------------------------
%% @private
%% @doc
%% Handling cast messages
%%
%% @end
%%--------------------------------------------------------------------
-spec(handle_cast(Request :: term(), State :: #state{}) ->
    {noreply, NewState :: #state{}} |
    {noreply, NewState :: #state{}, timeout() | hibernate} |
    {stop, Reason :: term(), NewState :: #state{}}).

handle_cast({'add_client', #client_info{client_addr = Addr, client_state = 'init'}=Client}, State) ->
    ets:insert('client', Client),
    {noreply, State};


handle_cast({'del_client',ClientAddr, ClientPort}, State) ->
    ets:delete('client', {ClientAddr, ClientPort}),
    {noreply, State};


handle_cast({'update_client',ClientAddr, ClientPort, Info}, State) ->
    'true' = ets:update_element('client', {ClientAddr, ClientPort}, {#client_info.client_info, Info}),
    {noreply, State};

handle_cast({'update_client_state',ClientAddr, ClientPort, NewState}, State) ->
    'true' = ets:update_element('client', {ClientAddr, ClientPort}, {#client_info.client_state, NewState}),
    {noreply, State};


handle_cast(_Request, State) ->
    {noreply, State}.

%%--------------------------------------------------------------------
%% @private
%% @doc
%% Handling all non call/cast messages
%%
%% @spec handle_info(Info, State) -> {noreply, State} |
%%                                   {noreply, State, Timeout} |
%%                                   {stop, Reason, State}
%% @end
%%--------------------------------------------------------------------
-spec(handle_info(Info :: timeout() | term(), State :: #state{}) ->
    {noreply, NewState :: #state{}} |
    {noreply, NewState :: #state{}, timeout() | hibernate} |
    {stop, Reason :: term(), NewState :: #state{}}).


handle_info('init', State) ->
    Tid = ets:new('client', [set,private, named_table, {keypos,#client_info.client_addr}]),
    {noreply, State#state{table_id = Tid}};


handle_info(_Info, State) ->
    {noreply, State}.

%%--------------------------------------------------------------------
%% @private
%% @doc
%% This function is called by a gen_server when it is about to
%% terminate. It should be the opposite of Module:init/1 and do any
%% necessary cleaning up. When it returns, the gen_server terminates
%% with Reason. The return value is ignored.
%%
%% @spec terminate(Reason, State) -> void()
%% @end
%%--------------------------------------------------------------------
-spec(terminate(Reason :: (normal | shutdown | {shutdown, term()} | term()),
    State :: #state{}) -> term()).
terminate(_Reason, _State) ->
    ok.

%%--------------------------------------------------------------------
%% @private
%% @doc
%% Convert process state when code is changed
%%
%% @spec code_change(OldVsn, State, Extra) -> {ok, NewState}
%% @end
%%--------------------------------------------------------------------
-spec(code_change(OldVsn :: term() | {down, term()}, State :: #state{},
    Extra :: term()) ->
    {ok, NewState :: #state{}} | {error, Reason :: term()}).
code_change(_OldVsn, State, _Extra) ->
    {ok, State}.

%%%===================================================================
%%% Internal functions
%%%===================================================================
