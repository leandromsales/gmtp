Ext.Loader.setConfig({
    enabled:true
});

Ext.define('FormItemTextoSimplesTempo', {
    extend:'Ext.form.field.Text',
    alias:'widget.' + Constantes.FORMITEM_TEXTO_SIMPLES_TEMPO,

    /*******************************
     ATRIBUTOS
     ********************************/

	labelSeparator:"",

    /*******************************
     INITCOMPONENT
     ********************************/

    initComponent:function () {

        var comp = this;
        this.superclass.initComponent.apply(this, arguments);
        this.addListeners();
        this.iniciarFormItem();

        // VALIDAÇÃO DA DATA - NÃO MODIFICAR
		/*
        Ext.apply(Ext.form.VTypes, {
            tempo:function (b, a) {
                comp.valido = comp.validar(b, comp);
                return comp.valido;
            },
            tempoText:"Tempo inválido"
        });
        */

        comp.plugins = [new InputTextMask('99:99:99', false)];

		/*
        Ext.apply(comp, {
            vtype:'tempo'
        });
        */

        //comp.callParent();
        //comp.validar("", this);
        
    },

    /*******************************
     MÉTODOS PRIVADOS
     ********************************/

    addListeners:function () {

        var comp = this;

        comp.on('change', function (obj) {
        	
        	var valido = comp.validar(comp.getValue(), this);
        	
            if (valido && obj.getValue() != "__:__:__")
                this.valor = obj.getValue();
            else
                this.valor = "";

            obj.fireEvent('emMudarValor', obj);
        });

        comp.on('keydown', function (obj, event) {
            if (event.keyCode == event.ENTER) {
                obj.fireEvent('emEnter', obj);
            }
        });

        comp.on('focus', function (obj) {
            obj.fireEvent('emFocusIn', obj);
        });

        comp.on('blur', function (obj) {
            if (obj.getValue() == "__:__:__")
                obj.setValue("");

            this.validar(this.getValue(), this);

            obj.fireEvent('emFocusOut', obj);
        });


    },

    iniciarFormItem:function () {

        var comp = this;

        //NOME
        this.name = this.atributo.item.nome;

        //OBRIGATORIO
        /*
        if (this.atributo.item.obrigatorio) {
            comp.afterLabelTextTpl = '<span style="color:red;font-weight:bold" data-qtip="Required">*</span>';
        }*/

        //DESCRICAO
        this.fieldLabel = this.atributo.item.descricao;

        //VALOR INICIAL
        if (this.atributo.item.valorInicial) {

            this.setValue(this.atributo.item.valorInicial);
            this.valor = this.atributo.item.valorInicial;
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

        this.validar("", this);
    },
    

    setEditar:function (item) {
		
		var valor = item[this.atributo.item.nome] + "";
		
		//contém caratectere :
		if(valor.indexOf(":") !== -1){

		}
		//se tempo está em segundos e não 
		else{
			valor = AjudaRenderer.tempo_renderer(valor);
		}
		
		// SETAR VALOR NO COMPONENTE
        this.setValue(valor);
        // SETAR VALOR NO FORMITEM
        this.valor = valor;

        this.validar(this.getValue(), this);
        
    },

    validar:function (e, form) {

        var comp = this;

        // SE O CAMPO ESTIVER VAZIO;
        if (e == "" || e == "__:__:__") {

            if (form.obrigatorio) {
                //form.markInvalid("Campo Obrigatório");
                comp.valido = false;
            } else {
                comp.valido = true;
            }

            //return comp.valido;

            // SE NÃO TIVER O CARACTERE '_'
        } else if (e.search("_") == -1) {

            // hrs = (e.substring(0, 2));
            min = (e.substring(3, 5));
            seg = (e.substring(6, 8));

            if ((min < 00) || (min > 59) || (seg < 00) || (seg > 59)) {
                comp.valido = false;
            } else {
                comp.valido = true;
            }

            //return comp.valido;

        // SE TIVER CARATERE '_'
        } else {
            comp.valido = false;
            //return comp.valido;
        }
        
        //ADICIONAR VALIDACAO VISUAL
        /*
        if (!comp.valido) {
			comp.markInvalid("Tempo Inválido");
		}*/
        
        this.setIconValido(this.valido);
        return comp.valido;
    },

    getValor:function () {

        return this.valor;
    },

    getSegundos:function () {
        if(this.getValido()){

            var tempo = this.valor;
            var a = tempo.split(':'); // split it at the colons
            var seconds = (+a[0]) * 60 * 60 + (+a[1]) * 60 + (+a[2]);
            return seconds;
        }else{
            return null;
        }
    },

    getValido:function () {

        //return this.valido;
        return this.validar(this.getValue(), this);
    },

    limpar : function() {

        this.setValue(null);
        this.valor = null;
        this.validar(this.getValue(), this);

    }
});
