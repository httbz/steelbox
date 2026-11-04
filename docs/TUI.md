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
- `h`/`l`: navegam horizontalmente quando a tela possuir controles nessa
  direção.
- `j`/`k`: movem a seleção para baixo/acima na lista.
- `Ctrl+N`/`Ctrl+P`: também movem a seleção para baixo/acima, no estilo
  Emacs.
- `Esc` ou `Backspace`: retornam à tela anterior; na lista, encerram a TUI.
- `Ctrl+B`: volta da tela de visualização para a lista.
- `Ctrl+L`: bloqueia o cofre.
- `Ctrl+O`: mostra a instrução para trocar de cofre.
- `Ctrl+Q`: encerra a aplicação.
- `Space` (`Spc`): abre a paleta de comandos rápidos da tela atual.
- `/` ou `Ctrl+F`: focam o terminal no modo de busca.
- `vault <caminho>`: troca o arquivo do cofre e retorna à tela de desbloqueio.
- `Ctrl+C`: interrupção do processo; use `Ctrl+Q` para sair normalmente.

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

### Terminal de comandos

A tela principal não possui botões de ação. Na parte inferior há um terminal
de comandos. Ele aceita comandos escritos por extenso e executa-os ao
pressionar Enter:

```text
edit
delete
new
view
search github
clear
change-master
vault C:\caminho\outro.vault
lock
quit
help
```

Pressionar `Space` mostra a paleta de comandos rápidos. Para buscar
rapidamente, pressione `/` ou `Ctrl+F`, digite o termo e pressione Enter:

```text
github
```

O terminal mostra apenas o termo `github`; a busca é executada internamente
quando Enter é pressionado. O texto `search` não é inserido no campo.

Pressionar `Space` mostra a paleta de comandos rápidos:

```text
Spc + E: Edit | Spc + D: Delete | Spc + N: New | Spc + V: View
```

Depois, pressione a letra indicada. Por exemplo, `Space`, `E` edita a entrada
selecionada. O texto digitado no terminal não é confundido com esses atalhos:
é possível escrever `edit` normalmente e pressionar Enter.

Quando o terminal está vazio, `H`, `J`, `K` e `L` continuam disponíveis para
navegação. Ao começar a escrever um comando, as letras passam a ser texto
normal do terminal.

### Controles

- **Busca**: é executada pelo comando `search <termo>` ou limpa com `clear`.
- **Lista**: mostra título, usuário e domínio das entradas filtradas.
- **Terminal**: mostra a sequência de teclas e recebe comandos completos.

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
- **Letras minúsculas (a-z)**: inclui letras minúsculas.
- **Letras maiúsculas (A-Z)**: inclui letras maiúsculas.
- **Números (0-9)**: inclui dígitos.
- **Símbolos**: inclui caracteres especiais.
- **Gerar**: cria uma nova senha.
- **Usar na entrada**: copia a senha gerada para o formulário atual.
- **Cancelar**: retorna ao formulário sem substituir a senha.

A senha gerada é exibida nessa tela para que possa ser conferida antes de ser
usada. Pelo menos um conjunto de caracteres deve permanecer selecionado; caso
contrário, o núcleo rejeita a geração.

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
