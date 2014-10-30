Ext.Loader.setConfig({
    enabled:true
});

Ext.define('js.form.item.FormItemListaOpcoes', {
    extend:'Ext.form.FieldContainer',
    alias:'widget.' + Constantes.FORMITEM_LISTA_OPCOES,

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
                itemId:'cmpComboListaOpcoes',
                xtype:'combo',
                flex:1,
                margin:'0 5 0 0',
                editable:false,
                selectOnTab:true,
                multiSelect:false,
                queryMode:'local',
                displayField:'descricao',
                valueField:'valor',
                emptyText:comp.atributo.item.info,

                store:{
                    fields:['descricao', 'valor'],
                    data:comp.atributo.item.componente.opcoes
                },

                listeners:{

                    select:function (obj, record) {
                        comp.valor = record[0].get('valor');
                        //console.log('Validando do evento select');
                        comp.validar();
                        comp.fireEvent('emMudarValor', comp);
                    },

                    keydown:function (obj, event) {
                        if (event.keyCode == event.ENTER) {
                            //console.log('Validando do evento enter');
                            comp.validar();
                            comp.fireEvent('emEnter', comp);
                        }
                    },

                    focus:function (obj) {
                        comp.fireEvent('emFocusIn', comp);
                    },

                    blur:function (obj) {
                        //console.log('Validando do evento blur');
                        comp.validar();
                        comp.fireEvent('emFocusOut', comp);
                    }
                }
            },
            {
                xtype:'button',
                itemId:'cmpLimpar',
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

        //OBRIGATÓRIO
        if (this.atributo.item.obrigatorio) {
            this.setObrigatorio(true);
        } else {
            this.setObrigatorio(false);
        }

        //VALOR INICIAL
        if (this.atributo.item.valorInicial) {
            comp.getComponent('cmpComboListaOpcoes').setValue(this.atributo.item.valorInicial);
            this.valor = this.atributo.item.valorInicial;
        }

        //console.log('Validando do initComponent');
        comp.validar();
    },

    setObrigatorio:function (obrigatorio) {

        if (obrigatorio == true) {
            this.obrigatorio = true;
        } else {
            this.obrigatorio = false;
            this.setFieldLabel(this.atributo.item.descricao + ':');
            //console.log('---> FormItemListaOpcoes obrigatorio: '+this.obrigatorio+ " fieldlabel: "+this.fieldLabel);
        }

        this.validar();
    },

    setIconValido:function (valido) {

        //console.log('valido '+valido);
        //console.log('obrigatorio '+this.obrigatorio);

        if (this.obrigatorio == true) {
            if (valido == true) {
                this.setFieldLabel(this.atributo.item.descricao + Constantes.ICONE_OBRIGATORIO_VALIDO_FORM_ITEM);
            } else {
                //this.setFieldLabel(this.atributo.item.descricao + Constantes.ICONE_OBRIGATORIO_NAO_VALIDO_FORM_ITEM);
            }
        }
    },

    setEditar:function (item) {

        var profundidade = "";

        if (this.atributo.item.profundidade) {
            profundidade = this.atributo.item.profundidade + ".";
        }

        //SETAR VALOR NO COMPONENTE
        var combo = this.getComponent('cmpComboListaOpcoes');

        //console.log("bean: "+this.atributo.item.componente.bean.toLowerCase());
        //console.log("nome: "+this.atributo.item.nome);

        combo.setValue(item[this.atributo.item.nome]);

        //SETAR VALOR NO FORMITEM
        this.valor = item[this.atributo.item.nome];
        //console.log(item);

        //console.log('Validando do setEdit');
        this.validar();
        
        this.fireEvent('emEditado', this);
    },

    /**
     * Método utilizado para validar FormItem;
     *
     * @param {} form
     * @param {} combo
     */
    validar:function () {

        //console.log('validando');
        //var combo = this.getComponent('cmpComboListaOpcoes');
        var form = this;

        if (form.valor != null) {
            form.conteudo = true;
        } else {
            form.conteudo = false;
        }

        if (form.obrigatorio) {
            form.valido = form.conteudo;
        } else {
            form.valido = true;
        }

        /*
         if (!form.valido) {
         combo.markInvalid("Invalido");
         } else {
         combo.clearInvalid();
         }
         */

        form.setIconValido(form.valido);
    },

    getValor:function () {
        return this.valor;
    },

    removerBotaoLimpar:function () {

        this.remove('cmpLimpar');

    },

    limpar:function () {

        this.valor = null;
        var combo = this.getComponent('cmpComboListaOpcoes')
        combo.clearValue();
        //console.log('Validando do botao limpar');
        this.validar();
    },

    setHabilitar:function (habilitar) {

        this.setDisabled(!habilitar);
        //console.log('Validando do setHabilitar');
        this.validar();
    },

    getCombo:function () {
        return this.getComponent('cmpComboListaOpcoes');
    },

    addOpcoes:function (opcoesAdicionar) {

        this.getComponent('cmpComboListaOpcoes').getStore().add(opcoesAdicionar)

    }

});
