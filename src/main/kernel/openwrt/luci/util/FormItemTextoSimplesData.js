Ext.Loader.setConfig({
    enabled:true
});

Ext.define('FormItemTextoSimplesData', {
    extend:'Ext.form.field.Date',
    alias:'widget.' + Constantes.FORMITEM_TEXTO_SIMPLES_DATA,

    /*******************************
     ATRIBUTOS
     ********************************/

    format:'d/m/Y',
    altFormats:'d/m/Y|j/n/Y|j/n/y|j/m/y|d/n/y|j/m/Y|d/m/Y|d-m-y|d-m-Y|d/m|d-m|dm|dmy|dmY|d|d-m-Y',
	labelSeparator:"",
	selectOnFocus: true,

    /*******************************
     INITCOMPONENT
     ********************************/

    initComponent:function () {

        this.superclass.initComponent.apply(this, arguments);
        this.addListeners();
        this.iniciarFormItem();

    },

    /*******************************
     MÉTODOS PRIVADOS
     ********************************/
    iniciarFormItem:function () {

        //NOME
        this.name = this.atributo.item.nome;

        //DESCRICAO
        this.fieldLabel = this.atributo.item.descricao;

        //VALOR INICIAL
        if (this.atributo.item.valorInicial == "true") {
            this.setValue(new Date());
        }

        //INFO
        this.emptyText = this.atributo.item.info;

        //OBRIGATÓRIO
        if (this.atributo.item.obrigatorio) {
            this.setObrigatorio(true);
        } else {
            this.setObrigatorio(false);
        }
    },
    
    /*******************************
     FUNÇÕES OBRIGATÓRIAS
     ********************************/

	setIconValido:function (valido) {

        if (this.obrigatorio == true && !this.hideLabel) {
            if (valido && this.isValid()) {
                this.setFieldLabel(this.atributo.item.descricao + Constantes.ICONE_OBRIGATORIO_VALIDO_FORM_ITEM);
            } else {
                this.setFieldLabel(this.atributo.item.descricao + Constantes.ICONE_OBRIGATORIO_NAO_VALIDO_FORM_ITEM);
            }
        }
    },

    setObrigatorio:function (obrigatorio) {

        if (obrigatorio == true) {
            this.obrigatorio = true;

        } else {
            this.obrigatorio = false;
            this.fieldLabel = this.atributo.item.descricao + ':';
        }

        this.validar();
    },

    addListeners:function () {

        var comp = this;

        comp.on('change', function (obj, newValue, oldValue) {

            this.valor = newValue;
            this.validar();
            obj.fireEvent('emMudarValor', obj);
        });

        comp.on('keydown', function (obj, event) {
            if (event.keyCode == event.ENTER) {
                obj.validar();
                obj.fireEvent('emEnter', obj);
            }
        });

        comp.on('focus', function (obj, event) {
        	
			//obj.selectText(0,2);        	
            obj.fireEvent('emFocusIn', obj);
        });

        comp.on('blur', function (obj, event) {
            obj.validar();
            obj.fireEvent('emFocusOut', obj);
        });
    },

    setEditar:function (item) {

        if (item[this.atributo.item.nome]) {

            var dataBanco = item[this.atributo.item.nome];
            var dateFormatted = Ext.widget(Constantes.AJUDA_DATA).stringSavedToDate(dataBanco);

            this.setValue(dateFormatted);
            this.valor = dateFormatted;
            this.validar();
        }
        else {
            this.setValue(null);
            this.valor = null;
            this.validar();
        }
        
        this.fireEvent('emEditado', this);
    },

    validar:function () {

        if (this.getValue()) {
            this.conteudo = true;
        } else {
            this.conteudo = false;
        }

        if (this.obrigatorio) {
            if(this.conteudo)
            	this.valido = this.isValid();
            else
            	this.valido = false;	
        } else {
            this.valido = this.isValid();
        }

       this.setIconValido(this.valido);
        
    },

    getValor:function () {

        if (this.getValue())
            return Ext.widget(Constantes.AJUDA_DATA).dateToStringAndSave(this.getValue());
        else
            return null;
    },

    getObjeto:function () {

        if (this.getValue())
            return this.getValue();
        else
            return null;
    },

    limpar : function() {

        this.setValue(null);
        this.valor = null;
        this.validar();

    }
});
