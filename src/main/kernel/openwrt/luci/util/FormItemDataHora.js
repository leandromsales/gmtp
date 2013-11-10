Ext.Loader.setConfig({
    enabled:true
});

Ext.define('FormItemDataHora', {
    extend:'Ext.form.FieldContainer',
    alias:'widget.' + 'dataHora',

    /*******************************
     ATRIBUTOS
     ********************************/

    layout:'hbox',
    labelSeparator:"",

    /*******************************
     INITCOMPONENT
     ********************************/

    initComponent:function () {

        var comp = this;

        this.items = [
            {
                itemId:'cmpData',
                xtype:Constantes.FORMITEM_TEXTO_SIMPLES_DATA,
                atributo:{
                    item:{
                        descricao:"",
                        obrigatorio:true,
                        info:"Informe a data",
                        nome: "data"
                    }
                },
                flex:1,
                margin:'0 5 0 0',
                hideLabel:true,
                labelAlign: 'top',
                listeners:{
                    emMudarValor:function (form) {
                        comp.fireEvent('emMudarValor', comp);
                        comp.validar();
                    }
                }
            },
            {
                itemId:'cmpHora',
                xtype:Constantes.FORMITEM_TEXTO_SIMPLES_TEMPO,
                atributo:{
                    item:{
                        descricao:"",
                        obrigatorio:true,
                        info:"Informe o tempo",
                        nome: "hora"
                    }
                },
                flex:1,
                margin:'0 5 0 0',
                hideLabel:true,
                listeners:{
                    emMudarValor:function (form) {
                        comp.fireEvent('emMudarValor', comp);
                        comp.validar();
                    }
                }
            },
            {
                xtype:'button',
                text:'Limpar',
                handler:function () {
                    comp.limpar();
                }
            }
        ];

        this.superclass.initComponent.apply(this, arguments);
        this.iniciarFormItem();
    },

    /*******************************
     MÉTODOS PRIVADOS
     ********************************/

    iniciarFormItem:function () {

        var comp = this;

        //VALOR INICIAL
        if (this.atributo.item.valorInicial) {
            //TODO
        }

        //OBRIGATÓRIO
        if (this.atributo.item.obrigatorio) {
            this.setObrigatorio(true);
        } else {
            this.setObrigatorio(false);
        }

        //comp.validar();
    },

    setObrigatorio:function (obrigatorio) {

        if (obrigatorio == true) {
            this.obrigatorio = true;
        } else {
            this.obrigatorio = false;
            //this.setFieldLabel(this.atributo.item.descricao + ':');
        }

        this.validar();
    },

    setIconValido:function (valido) {

        if (this.obrigatorio == true) {
            if (valido == true) {
                this.setFieldLabel(this.atributo.item.descricao + Constantes.ICONE_OBRIGATORIO_VALIDO_FORM_ITEM);
            } else {
                this.setFieldLabel(this.atributo.item.descricao + Constantes.ICONE_OBRIGATORIO_NAO_VALIDO_FORM_ITEM);
            }
        }
    },

    validar:function () {

        var comp = this;
        var formData = comp.getComponent('cmpData');
        var formTempo = comp.getComponent('cmpHora');

        console.log('%% Validação formdata e hora ' + formData.getValido() + formTempo.getValido());

        if (formData.getValido() && formTempo.getValido()) {
            comp.valido = true;
        } else {
            comp.valido = false;
        }

        comp.setIconValido(comp.valido);
    },

    setEditar:function (item) {
		
		console.log("@@ Editando dataHora");
		console.log(item);
		
        //TODO Implementar o setEditar após ser realizada a criação;
        if (item[this.atributo.item.nome]) {

            var dataBanco = item[this.atributo.item.nome];
            var dateFormatted = Ext.widget(Constantes.AJUDA_DATA).stringParaDataHora(dataBanco);

            console.log('##Editar DataHora');
            console.log(dateFormatted);

            var formData = this.getComponent('cmpData');
            var formTempo = this.getComponent('cmpHora');

            formData.setEditar({data: Ext.Date.format(dateFormatted, 'Y-m-d') });
            formTempo.setEditar({hora: Ext.Date.format(dateFormatted, 'H:i:s') });

            this.validar();
        }
        
        this.fireEvent('emEditado', this);
    },

    getValido:function () {

        var formData = this.getComponent('cmpData');
        var formTempo = this.getComponent('cmpHora');

        if (formData.getValido() && formTempo.getValido()) {
            return true;
        } else {
            return false;
        }
    },

    getValor:function () {

        var formData = this.getComponent('cmpData');
        var formTempo = this.getComponent('cmpHora');

        if (this.getValido()) {

            var valor = formData.getValor() + " " + formTempo.getValor();
            console.log("--> Valor form item datahora " + valor);
            return valor;
        } else {
            return null;
        }
    },

    limpar:function () {

        this.getComponent('cmpData').limpar();
        this.getComponent('cmpHora').limpar();
    }
});
