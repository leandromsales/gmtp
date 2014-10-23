Ext.define('Constantes', {
	alias : 'widget.constantes',
	singleton : true,


    //URL_SERVIDOR : "http://lex.mottaesoares.com.br:8080/lex-test/",
    //URL_SERVIDOR_PROCESS : "http://lex.mottaesoares.com.br:8080/lex-test/process/",
    //URL_SERVIDOR : "http://localhost:8080/lex/",
    //URL_SERVIDOR_PROCESS : "http://localhost:8080/lex/process/",
    //URL_SERVIDOR : "http://192.168.50.80:8080/lex/",
    //URL_SERVIDOR_PROCESS : "http://192.168.50.80:8080/lex/process/",
    URL_SERVIDOR_PROCESS : "../../process/",
    URL_SERVIDOR_SIMPLE_REPORT : "../../simpleReport/",
    URL_CAPA_PROCESSO : "../../capaProcesso",
    URL_EXTRATO_SERVICO : "../../extratoServico",
    URL_EXTRATO_REQUISICOES : "../../extratoRequisicoes",


	/**
	 * 
	 * ESPECIFICAÇÃO DE BUSCA
	 * 
	 */

	/** CONDIÇÕES * */
	AND : "AND",
	OR : "OR",

	/** MODOS LIKE * */
	ANY : 0,
	LEFT : 1,
	RIGHT : 2,

	/** SINAIS * */
	EQUALTO : "=",
	GRATERTHAN : ">",
	GRATERTHAN_EQUALTO : ">=",
	LESSTHAN : "<",
	LESSTHAN_EQUALTO : "<=",
	NOTEQUALTO : "<>",
	LIKE : "LIKE",

	/** DIREÇÃO * */
	ASC : "ASC",
	DESC : "DESC",

	/** RESTRICT * */
	CARACTERES_PERMITIDOS_DESCRICAO : "descricao",
    CARACTERES_PERMITIDOS_NUMEROS_SIMPLES : "numeroSimples",
    CARACTERES_PERMITIDOS_NUMEROS_FRACIONADOS : "numeroFracionado",
    CARACTERES_PERMITIDOS_TODOS : "todos",

	/**
	 * 
	 * FORMULÁRIO
	 * 
	 */
	
	/** DONE FORM ITEMS **/
	FORMITEM_TEXTO_SIMPLES : "TextoSimples",
	FORMITEM_TEXTO_SIMPLES_SENHA : "TextoSimplesSenha",
	FORMITEM_TEXTO_SIMPLES_CPF : "TextoSimplesCPF",
	FORMITEM_TEXTO_SIMPLES_CNPJ : "TextoSimplesCNPJ",
	FORMITEM_CHECKBOX : "CheckBox",
	FORMITEM_TELEFONE : "Telefone",
	FORMITEM_TEXTO_SIMPLES_DATA : "TextoSimplesData",
	
	FORMITEM_TEXTO_SIMPLES_TEMPO : "TextoSimplesTempo",
	FORMITEM_TEXTO_SIMPLES_CEP : "TextoSimplesCEP",
	FORMITEM_ENDERECO : "Endereco",
	FORMITEM_TEXTO_SIMPLES_DINHEIRO : "TextoSimplesDinheiro",
	FORMITEM_TEXTO_AREA : "TextoArea",
	FORMITEM_LISTA_OPCOES : "ListaOpcoes",
	FORMITEM_CALENDARIO : "Calendario",
	FORMITEM_TEXTO_SIMPLES_DOCUMENTO : "TextoSimplesDocumento",
	FORMITEM_LISTA_SIMPLES : "ListaSimples",
	FORMITEM_LISTA_CONTRATO_HONORARIO : "ListaContratoHonorario",
    FORMITEM_LISTA_COLECAO : "ListaColecao",
    FORMITEM_TEXTO_RICO: "TextoRico",
	
	/** UNDONE FORM ITEMS **/
	FORMITEM_LISTA_AVANCADA : "ListaAvancada",
	FORMITEM_ROTAS : "Rotas",
	FORMITEM_NONE : "None",
    FORMITEM_DATA : "Data",

    //FORMITEM ADICIONADO PARA O FORMULARIO DE ATOS
    FORMITEM_DATA_HORA:"DataHora",
    FORMITEM_SERVICO_CONTRATADO:"ServicoContratado",
    FORMITEM_LEMBRETE:"Lembrete",
    FORMITEM_HISTORICO:"Historico",


	/** AÇÕES * */
	//ACAO_FORMULARIO_CRIAR : 'AcaoFormularioCriar',
	//ACAO_FORMULARIO_EDITAR : 'AcaoFormularioEditar',
	//ACAO_FORMULARIO_BUSCAR : 'AcaoFormularioBuscar',
	ACAO_BASE : 'AcaoBase',
	FORM_BUSCA_BASE: 'FormBuscaBase',
	//ACAO_ADIANTAMENTO_RETIRAR: 'retirar_adiantamento', // AQUI FICA O VALOR DA ACAO QUE ESTA LEXSETUP
	//ACAO_COMBUSTIVEL_RETIRAR: 'AcaoCombustivelRetirar',
	//ACAO_COPIA_CONCLUIR:'AcaoCopiaConcluir',
	//ACAO_MATERIALESCRITORIO_RETIRAR:'AcaoMaterialEscritorioRetirar',
	
	/**
	 * 
	 * AJUDA
	 * 
	 */

	AJUDA_ACAO : "AjudaAcao",
	AJUDA_MODELO : "AjudaModelo",
	AJUDA_JSON : "AjudaJSon",
	AJUDA_SEARCH_SPECS : "AjudaSearchSpecs",
    AJUDA_ATRIBUTO : "AjudaAtributo",
    AJUDA_NOTIFICACAO: "AjudaNotificacao",
    AJUDA_RENDERER: "AjudaRenderer",
    AJUDA_INTERFACE: "AjudaInterface",
    AJUDA_DADOS: "AjudaDados",
    AJUDA_EXTRA: "AjudaExtra",
    AJUDA_DATA: "AjudaData",
    AJUDA_DINHEIRO: "AjudaDinheiro",
	

	/**
	 * 
	 * GERAL
	 * 
	 */

	TIPO_BASICO : "Basico",
	TIPO_EXTRA : "Extra",
	TIPO_MUDAR_VISAO : "MudarVisao",
	TIPO_PROTOCOLO : "Protocolo",
    TIPO_RELATORIO : "Relatorio",

	/**
	 * 
	 * AÇÕES BÁSICAS
	 * 
	 */
	
	ACAO_BASICA_ACESSAR : "acessar",
	ACAO_BASICA_LISTAR_TODOS : "listar_todos",
	ACAO_BASICA_CRIAR : "criar",
    ACAO_BASICA_CRIAR_MODELO : "criar_modelo",
	ACAO_BASICA_EDITAR : "editar",
	ACAO_BASICA_COPIAR : "copiar",
	ACAO_BASICA_REMOVER : "remover",
	ACAO_BASICA_EXCLUIR : "excluir",
	ACAO_BASICA_IMPRIMIR : "imprimir",
	ACAO_BASICA_EXIBIR_LOG : "exibir_log",
	ACAO_BASICA_LIXEIRA : "lixeira",
	
	/**
	 * 
	 * MIXINS
	 * 
	 */
	
	FORM_ITEM_BASE : 'js.form.item.FormItemBase',
	ACAO_BASE : 'js.acao.AcaoBase',
	
	/**
	 * 
	 * MENSAGENS DE ALERT
	 * 
	 */
	
	SELECIONE_UM_ITEM: 'Selecione um item da lista para realizar essa ação',
	FALHA_EXECUTAR_ACAO: 'Falha durante a execução da ação',
    ERROR_STRING_CAMPO_OBRIGATORIO : "Campo Obrigatório",
    TEXTO_INFORMACAO_LOADING: "Carregando Informações...",

    /**
     *
     * TIPOS DE CLASSES DO SISTEMA
     *
     */

    //TODO ANALISAR E REMOVER ESSA CLASSE;
    ATRIBUTO_CLASSE_AUTO : 'auto',

    /**
     * BASICOS
     */
    ATRIBUTO_CLASSE_STRING : 'string',
    ATRIBUTO_CLASSE_INT : 'int',
    ATRIBUTO_CLASSE_FLOAT : 'float',
    ATRIBUTO_CLASSE_BOOLEAN : 'boolean',
    ATRIBUTO_CLASSE_DATE : 'date',

    /**
     * NAO BASICOS
     */
    ATRIBUTO_CLASSE_MOEDA : 'moeda',
    ATRIBUTO_CLASSE_PORCENTAGEM : 'porcentagem', //TODO ANALISAR USO, NÃO UTILZIADO, POSSIVEL DUPLICATA COM FLOAT;
    ATRIBUTO_CLASSE_DURACAO : 'duracao', //USADO PARA ATRIBUTOS DO QUE EXIBEM HORARIOS, POR EXEMPLO 00:00:00
    ATRIBUTO_CLASSE_LISTA : 'lista',
    ATRIBUTO_CLASSE_ROTA : 'rota',
    ATRIBUTO_CLASSE_DATA_HORA : 'datahora',
    ATRIBUTO_CLASSE_TELEFONE : 'telefone',

    /**
     *
     * ÍCONES PARA OBRIGATORIO EM FORMITEMS
     *
     */
    ICONE_OBRIGATORIO_VALIDO_FORM_ITEM: ': <img width="13" height="13" src="images/icons/varios/16/ok.png"/>',
    ICONE_OBRIGATORIO_NAO_VALIDO_FORM_ITEM: ': <img width="13" height="13" src="images/icons/ashung/16/error.png"/>',

    /**
     *
     * STATUS DE PROTOCOLO
     *
     */
    STATUS_PROTOCOLO_DISPONIVEL: 0,
    STATUS_PROTOCOLO_FINALIZADO: 0,
    STATUS_PROTOCOLO_SOLICITADO: 1,
    STATUS_PROTOCOLO_RETIRADO: 2,
    
    /**
     * 
     * 
     */
	FORMULARIO_ALTURA_PADRAO_JANELA: 400,
	FORMULARIO_LARGURA_PADRAO_JANELA: 650,

    /**
     * VALORES DEFAULT PARA TIPOS NO SISTEMA
     */
    VALOR_DEFAULT_REMOVER_CLASSE_DATE: '1900-01-01',
    VALOR_DEFAULT_REMOVER_RELACIONAMENTO: -1,

    ID_ACAO_VISUALIZAR_INFO: 84

});