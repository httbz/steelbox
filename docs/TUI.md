# Interface TUI

Este documento descreve a interface de terminal do Steelbox (`steelbox_tui`).
O cofre continua sendo local e todas as alterações são gravadas de forma
criptografada usando a senha mestra atual.

## Iniciar

No Windows:

```powershell
.\build-tui\steelbox_tui.exe --vault .\steelbox.vault
```

O argumento `--vault` é opcional. Quando omitido, a TUI procura
`steelbox.vault` no diretório atual.

## Navegação geral

- `Tab`: próximo controle.
- `Shift+Tab`: controle anterior, quando suportado pelo terminal.
- `Enter` ou `Espaço`: ativa o botão selecionado.
- `Esc`: volta da visualização para a lista ou sai da lista.
- `j`/`k`: move a seleção para baixo/acima na lista.
- `Ctrl+N`/`Ctrl+P`: move a seleção para baixo/acima na lista no estilo Emacs.
- `v`: visualiza a entrada selecionada.
- `e`: edita a entrada selecionada.
- `d`: abre a confirmação de exclusão.
- `n`: abre o formulário de nova entrada.
- `s`: executa a busca digitada.
- `q`: sai da tela de entradas.
- `Ctrl+C`: interrupção do processo; use os botões ou `Esc` para sair
  normalmente.

Os campos de senha são mascarados. A senha mestra e as senhas das entradas não
são exibidas como texto durante a digitação.

## Tela de desbloqueio

### Como chegar

É a primeira tela ao iniciar a TUI. Também aparece depois de usar
**Bloquear**.

### Para que serve

Autentica a senha mestra e carrega o cofre criptografado.

### Controles

- **Senha mestra**: campo de entrada mascarado.
- **Desbloquear**: tenta carregar o arquivo indicado.
- **Sair**: fecha a TUI sem desbloquear o cofre.

Uma senha incorreta ou um arquivo adulterado não desbloqueia o cofre. A
mensagem de erro aparece na própria tela.

## Tela de entradas

### Como chegar

É exibida após o desbloqueio bem-sucedido.

### Para que serve

É a tela principal para pesquisar e administrar as entradas.

### Controles

- **Busca**: recebe o texto da busca; o filtro só é aplicado ao ativar
  **Buscar** ou pressionar `s`.
- **Lista**: mostra título, usuário e domínio das entradas filtradas.
- **Visualizar**: abre a entrada selecionada.
- **Nova entrada**: abre o formulário de criação.
- **Editar**: abre o formulário da entrada selecionada.
- **Excluir**: abre a confirmação de exclusão.
- **Bloquear**: bloqueia o cofre e volta ao login.
- **Alterar senha mestra**: abre o formulário de troca da senha.
- **Sair**: encerra a aplicação.

### Atalhos

- `N`: abre **Nova entrada**.
- `E`: abre **Editar** para a entrada selecionada.
- `Enter`: pode ser usado no botão **Visualizar**.
- `Esc`: bloqueia a tela atual e encerra a aplicação.

## Tela de visualização

### Como chegar

Selecione uma entrada na tela principal e ative **Visualizar**.

### Para que serve

Exibe os dados não secretos da entrada:

- ID;
- título;
- usuário;
- URL;
- domínio normalizado;
- quantidade de tags;
- notas.

A senha permanece mascarada como `********` até o usuário ativar
**Visualizar senha**.

### Controles

- **Editar**: abre o formulário de edição.
- **Excluir**: abre a confirmação de exclusão.
- **Visualizar senha**: mostra ou oculta a senha na tela.
- **Voltar**: retorna à lista.

`Esc` também retorna à lista.

## Tela de nova entrada

### Como chegar

Na tela principal, ative **Nova entrada** ou pressione `N`.

### Campos

- Título;
- Usuário;
- URL;
- Senha;
- Notas;
- Tags separadas por vírgula.

O título, notas e tags podem ficar vazios. A validação do núcleo exige URL,
usuário e senha válidos.

### Controles

- **Gerar senha**: preenche o campo de senha com uma senha criptograficamente
  aleatória.
- **Visualizar senha**: alterna a exibição da senha digitada ou gerada.
- **Salvar**: valida a entrada, adiciona-a ao cofre e salva o arquivo.
- **Cancelar**: abandona o formulário sem modificar o cofre.

O botão **Gerar senha** abre a tela completa de geração de senha. A tecla `g`
não abre o gerador enquanto o foco estiver em um campo de texto.

## Tela de edição

### Como chegar

Selecione uma entrada e ative **Editar**, ou pressione `E` na tela principal.

### Para que serve

Permite alterar os dados da entrada existente. Os campos são preenchidos com
os valores atuais e a senha começa mascarada.

O botão **Visualizar senha**, ao lado do campo de senha, alterna entre a senha
mascarada e a senha visível.

### Salvamento

**Salvar** executa a validação do núcleo, preserva a data de criação, atualiza
a data de modificação e grava o cofre criptografado. **Cancelar** descarta as
alterações feitas no formulário.

## Tela de confirmação de exclusão

### Como chegar

Selecione uma entrada e ative **Excluir** na lista ou na tela de visualização.

### Para que serve

Evita exclusões acidentais. A entrada só é removida depois que o usuário
digita exatamente:

```text
EXCLUIR
```

Depois da confirmação, a entrada é removida do cofre, o arquivo é salvo e a
lista é atualizada. **Cancelar** não altera os dados.

## Tela de geração de senha

### Como chegar

No formulário de nova entrada ou edição, pressione `G`.

### Para que serve

Gera uma senha usando o gerador seguro do núcleo e permite usá-la no campo de
senha da entrada.

### Controles

- **Comprimento**: valor entre 4 e 1024, com padrão 20.
- **Gerar**: cria uma nova senha.
- **Usar na entrada**: copia a senha gerada para o formulário atual.
- **Cancelar**: retorna ao formulário sem substituir a senha.

A senha gerada é exibida nessa tela para que possa ser conferida antes de ser
usada.

## Tela de alteração da senha mestra

### Como chegar

Na tela principal, ative **Alterar senha mestra**.

### Para que serve

Regrava o mesmo conteúdo do cofre usando uma nova senha mestra.

### Controles

- **Nova senha**;
- **Confirme a nova senha**;
- **Alterar senha**;
- **Cancelar**.

As duas senhas precisam ser preenchidas e iguais. O arquivo só é substituído
depois que a cifragem e o salvamento forem concluídos com sucesso.

## Segurança e persistência

- O cofre é descriptografado somente depois da autenticação.
- A senha mestra não é armazenada no arquivo.
- Alterações usam o backup e a substituição atômica implementados pelo núcleo.
- Bloquear limpa a sessão do cofre e retorna à tela de login.
- Sair bloqueia o cofre antes de encerrar.
- A senha da entrada não é exibida na tela de detalhes.

## Limitações atuais

- Ainda não há bloqueio automático por inatividade.
- A TUI ainda não copia senhas para a área de transferência.
- A senha da entrada pode ser visualizada apenas no formulário mascarado ou na
  tela de geração; a revelação temporária na tela de detalhes ainda não foi
  implementada.
