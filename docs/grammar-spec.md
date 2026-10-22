# Especificação da Gramática e Arquitetura da ArgosLang

> Especificação de evolução — 14 de setembro de 2026
> Parser-alvo: LR canônico com um símbolo de lookahead, LR(1).

> **Estado do compilador (22 de outubro de 2026):** o parser implementado é
> manual, com precedência de operadores; não é LR(1). Esta especificação
> descreve o desenho futuro e não deve ser usada como lista de recursos
> executáveis. Para o subconjunto que funciona hoje, consulte [mvp.md](mvp.md).
> A grafia canônica é `function`, `var`/`var mut`, `nil` e tipos primitivos em
> minúsculas (`int`, `bool`, `string`); exemplos antigos nesta especificação
> não substituem essa forma canônica.

## 1. Escopo

Este documento define a sintaxe, a gramática formal e as regras semânticas centrais da ArgosLang. As palavras **deve**, **não deve**, **pode** e **requer** possuem sentido normativo. Uma regra documentada não constitui prova de implementação.

Termos:

- **CST:** árvore concreta, adequada a ferramentas e preservação de tokens;
- **AST:** árvore abstrata independente do algoritmo de parsing;
- **HIR:** representação de alto nível resolvida e tipada;
- **IR Argos:** representação intermediária própria;
- **Machine IR:** representação específica de uma arquitetura;
- **executor:** fronteira para código C, C++, Rust, Swift ou Assembly.

## 2. Filosofia

A ArgosLang é estaticamente e fortemente tipada, multiparadigma e orientada a objetos, com recursos funcionais, null-safety, imutabilidade por padrão e memória determinística sem garbage collector obrigatório.

Ela possui:

1. sintaxe moderna inspirada em Swift;
2. classes como tipos com identidade e herança;
3. interfaces como contratos comportamentais;
4. protocols como agregados imutáveis equivalentes conceitualmente aos records do Java moderno;
5. generics e constraints;
6. perfis hosted, embedded, freestanding e bare metal;
7. compilação nativa direta, sem C, C++ ou LLVM intermediários;
8. bytecode próprio para execução em VM;
9. interoperabilidade com C, C++, Rust, Swift e Assembly.

## 3. Pipeline

```text
Fonte → Lexer → tokens → Parser LR(1) → CST/AST → HIR tipada → IR Argos
                                                             ├─→ Bytecode → VM
                                                             └─→ Machine IR → máquina
```

C, C++, Rust e Swift não são destinos da ArgosLang. Seus executores são portas anexas de interoperabilidade.

## 4. Léxico

Arquivos usam UTF-8. Identificadores aceitam letras Unicode, `_` e dígitos depois do primeiro caractere.

```ebnf
Identifier ::= IdentifierStart IdentifierContinue* ;
IdentifierStart ::= "_" | UnicodeLetter ;
IdentifierContinue ::= IdentifierStart | DecimalDigit ;

IntegerLiteral ::= DecimalInteger | HexInteger | BinaryInteger | OctalInteger ;
FloatLiteral ::= DecimalFloat FloatSuffix? ;
CharLiteral ::= "'" CharContent "'" ;
StringLiteral ::= '"' StringContent* '"' ;
MultilineStringLiteral ::= '"""' MultilineContent* '"""' ;
BooleanLiteral ::= "true" | "false" ;
NullLiteral ::= "nil" ;
```

Comentários de linha começam com `//`; comentários de bloco usam `/* ... */` e podem aninhar. Comentários de documentação usam `///` ou `/** ... */`.

Separadores `_` podem aparecer entre dígitos, nunca nas extremidades ou duplicados.

### 4.1. Terminadores

O lexer produz o token `SEMI` para `;` escrito ou para newline elegível. Newline não gera `SEMI` dentro de `()`, `[]`, listas incompletas ou após operador que exige operando. O terminador também pode ser omitido imediatamente antes de `}` ou `EOF` (e antes de `else` após um corpo de uma instrução). Instruções adjacentes na mesma linha ainda exigem separação.

Isso substitui o predicado `&"}"` da versão antiga, pois ele é sintaxe de PEG, não EBNF/LR.

## 5. Contrato LR(1)

O parser é LR(1) canônico. Estados com contextos diferentes não devem ser fundidos como em LALR. Recursão à esquerda é permitida e preferida para operadores e listas associativas à esquerda.

O gerador deve terminar com zero conflitos shift/reduce e reduce/reduce. Precedência só pode resolver um conflito quando a regra for normativa. Recuperação deve sincronizar em `SEMI`, `}`, início de declaração e palavras-chave de fluxo.

A CST pode preservar pontuação e comentários. A AST representa significado estrutural. Não existe “AST LR”: LR caracteriza o parser, não a árvore.

## 6. Unidade de compilação

```ebnf
CompilationUnit ::= ModuleDecl? ImportDecl* TopLevelDecl* EOF ;
ModuleDecl ::= AttributeList? Visibility? "module" QualifiedName SEMI ;
ImportDecl ::= AttributeList? Visibility? "import" QualifiedName ImportTail? SEMI ;
ImportTail ::= "." "*" | "." "{" ImportItemList "}" | "as" Identifier ;
ImportItemList ::= ImportItem ("," ImportItem)* ","? ;
ImportItem ::= Identifier ("as" Identifier)? ;
QualifiedName ::= Identifier ("." Identifier)* ;

TopLevelDecl ::= ClassDecl | ProtocolDecl | InterfaceDecl | EnumDecl
               | TypeAliasDecl | FunctionDecl | VariableDecl | ForeignDecl ;
TypeAliasDecl ::= AttributeList? Visibility? "type" Identifier
                  GenericParameterClause? "=" Type SEMI ;
```

Instruções no topo são exclusivas de scripts. Módulos compilados contêm declarações e uma entrada válida.

## 7. Atributos, acesso e modificadores

```ebnf
AttributeList ::= Attribute+ ;
Attribute ::= "@" QualifiedName ("(" ArgumentList? ")")? ;
Visibility ::= "public" | "internal" | "protected" | "private" ;
ClassModifier ::= "abstract" | "sealed" | "final" | "unsafe" ;
MemberModifier ::= "abstract" | "final" | "override" | "static"
                 | "mut" | "unsafe" | "async" | "native" ;
```

- `public`: acessível por módulos importadores;
- `internal`: acessível no módulo e padrão para declarações de topo;
- `protected`: classe e subclasses; somente membros;
- `private`: escopo nominal que declarou o membro;
- `sealed class`: subclasses somente no mesmo módulo;
- `final class`: não pode ser estendida;
- `final` em membro: não pode ser sobrescrito;
- `abstract`: exige implementação concreta;
- `override`: obrigatório em toda sobrescrita;
- `sealed final` é redundante e inválido.

## 8. Classes

`class` é o único tipo nominal com identidade, estado e herança de implementação. Não existe `struct`; ArgosLang não replica a duplicidade quase equivalente de `class` e `struct` do C++.

```ebnf
ClassDecl ::= AttributeList? Visibility? ClassModifier*
              "class" Identifier GenericParameterClause?
              ClassInheritanceClause? WhereClause? ClassBody ;
ClassInheritanceClause ::= ":" NamedType ("," NamedType)* ;
ClassBody ::= "{" ClassMember* "}" ;

ClassMember ::= FieldDecl | PropertyDecl | InitializerDecl
              | DeinitializerDecl | MethodDecl | OperatorDecl
              | NestedTypeDecl | ForeignDecl | EmptyDecl ;

FieldDecl ::= AttributeList? Visibility? MemberModifier*
              "var" ("mut")? Identifier ":" Type
              ("=" Expression)? SEMI ;

PropertyDecl ::= AttributeList? Visibility? MemberModifier*
                 ("let" | "var") Identifier ":" Type PropertyBody ;
PropertyBody ::= "{" PropertyAccessor+ "}" ;
PropertyAccessor ::= Visibility? "get" (SEMI | FunctionBody)
                   | Visibility? "set" ("(" Identifier ")")?
                     (SEMI | FunctionBody) ;

InitializerDecl ::= AttributeList? Visibility? MemberModifier*
                    "init" GenericParameterClause?
                    "(" ParameterList? ")" ThrowsClause?
                    WhereClause? FunctionBody ;
DeinitializerDecl ::= AttributeList? "deinit" "(" ")" FunctionBody ;

MethodDecl ::= AttributeList? Visibility? MemberModifier*
               FunctionSignature (FunctionBody | SEMI) ;

ComputedPropertyDecl ::= AttributeList? Visibility? MemberModifier*
                         "var" ("mut")? Identifier ":" Type PropertyBody ;
NestedTypeDecl ::= ClassDecl | ProtocolDecl | InterfaceDecl | EnumDecl
                 | TypeAliasDecl ;
OperatorDecl ::= AttributeList? Visibility? MemberModifier*
                 "operator" OperatorToken "(" ParameterList? ")"
                 ReturnClause? WhereClause? FunctionBody ;
OperatorToken ::= "+" | "-" | "*" | "/" | "%" | "==" | "!="
                | "<" | "<=" | ">" | ">=" ;
EmptyDecl ::= SEMI ;
```

Uma classe estende no máximo uma classe e implementa múltiplas interfaces. A classe-base, se houver, é o primeiro item. Todos os campos obrigatórios devem estar inicializados ao final de `init`. `deinit` é único e determinístico.

Corpos ausentes só são válidos em membro abstrato, requisito de interface ou declaração estrangeira.

## 9. Protocols de dados

Na ArgosLang, `protocol` significa agregado nominal imutável, equivalente conceitual ao `record` do Java 17–25. Não significa interface no sentido do Swift; esse papel pertence a `interface`.

```ebnf
ProtocolDecl ::= AttributeList? Visibility? "protocol" Identifier
                 GenericParameterClause?
                 "(" ProtocolComponentList? ")"
                 ProtocolConformanceClause? WhereClause?
                 (ProtocolBody | SEMI) ;
ProtocolComponentList ::= ProtocolComponent ("," ProtocolComponent)* ","? ;
ProtocolComponent ::= AttributeList? Visibility? Identifier ":" Type
                      ("=" Expression)? ;
ProtocolConformanceClause ::= ":" NamedType ("," NamedType)* ;
ProtocolBody ::= "{" ProtocolMember* "}" ;
ProtocolMember ::= ComputedPropertyDecl | MethodDecl
                 | NestedTypeDecl | EmptyDecl ;
```

Um `protocol` possui implicitamente:

- componentes finais e imutáveis;
- inicializador canônico;
- igualdade e hashing estruturais;
- representação textual estrutural;
- decomposição por pattern matching;
- proibição de herança de implementação;
- possibilidade de implementar interfaces;
- impossibilidade de adicionar campos ocultos no corpo.

`protocol` é implicitamente final e não aceita `abstract`, `sealed` ou `final`.

```argos
public protocol User<TId>(
    id: TId,
    name: String,
    email: String?
) : Serializable where TId: Hashable;
```

## 10. Interfaces

`interface` define contrato comportamental sem estado armazenado.

```ebnf
InterfaceDecl ::= AttributeList? Visibility? "interface" Identifier
                  GenericParameterClause? InterfaceInheritanceClause?
                  WhereClause? InterfaceBody ;
InterfaceInheritanceClause ::= ":" NamedType ("," NamedType)* ;
InterfaceBody ::= "{" InterfaceMember* "}" ;

InterfaceMember ::= InterfaceMethodRequirement
                  | InterfacePropertyRequirement
                  | AssociatedTypeDecl | DefaultMethodDecl
                  | NestedTypeDecl | EmptyDecl ;
InterfaceMethodRequirement ::= AttributeList? Visibility?
                               FunctionSignature SEMI ;
InterfacePropertyRequirement ::= AttributeList? Visibility?
                                 "var" ("mut")? Identifier ":" Type
                                 "{" AccessorRequirementList "}" ;
AccessorRequirementList ::= "get" "set"? ;
AssociatedTypeDecl ::= "type" Identifier GenericBoundList? SEMI ;
DefaultMethodDecl ::= AttributeList? Visibility?
                      FunctionSignature FunctionBody ;
```

Interfaces podem herdar múltiplas interfaces. Implementações padrão não armazenam estado. Conformidade é nominal.

## 11. Enums

```ebnf
EnumDecl ::= AttributeList? Visibility? "enum" Identifier
             GenericParameterClause? EnumConformanceClause?
             WhereClause? EnumBody ;
EnumConformanceClause ::= ":" NamedType ("," NamedType)* ;
EnumBody ::= "{" EnumMember* "}" ;
EnumMember ::= EnumCaseDecl | MethodDecl | ComputedPropertyDecl
             | NestedTypeDecl | EmptyDecl ;
EnumCaseDecl ::= AttributeList? "case" EnumCaseElementList SEMI ;
EnumCaseElementList ::= EnumCaseElement ("," EnumCaseElement)* ;
EnumCaseElement ::= Identifier EnumPayload? ("=" ConstantExpression)? ;
EnumPayload ::= "(" TypeList? ")" | "(" NamedTypeList ")" ;
NamedTypeList ::= NamedTypeElement ("," NamedTypeElement)* ","? ;
NamedTypeElement ::= Identifier ":" Type ;
```

Enums podem ser conjuntos simples ou variantes com payload.

## 12. Generics

```ebnf
GenericParameterClause ::= "<" GenericParameterList ">" ;
GenericParameterList ::= GenericParameter ("," GenericParameter)* ","? ;
GenericParameter ::= Identifier GenericBoundList? DefaultTypeArgument? ;
GenericBoundList ::= ":" GenericBound ("&" GenericBound)* ;
GenericBound ::= NamedType ;
DefaultTypeArgument ::= "=" Type ;

GenericArgumentClause ::= "<" GenericArgumentList ">" ;
GenericArgumentList ::= Type ("," Type)* ","? ;

WhereClause ::= "where" WherePredicateList ;
WherePredicateList ::= WherePredicate ("," WherePredicate)* ;
WherePredicate ::= Type ":" GenericBound ("&" GenericBound)*
                 | Type "==" Type ;
```

Constraints após `:` expressam conformidade. `&` combina limites. `where` expressa relações adicionais. O compilador pode monomorfizar ou compartilhar código, sem alterar a semântica.

Para evitar o conflito entre argumentos genéricos e operadores `<`/`>` em contexto de expressão, especialização explícita usa `::<...>`:

```argos
let value = parse::<Int>(text);
```

## 13. Funções e parâmetros

```ebnf
FunctionDecl ::= AttributeList? Visibility? MemberModifier*
                 FunctionSignature SimpleFunctionBody ;
FunctionSignature ::= "function" Identifier GenericParameterClause?
                      "(" ParameterList? ")"
                      ReturnClause? ThrowsClause? WhereClause? ;
ReturnClause ::= "->" Type ;
ThrowsClause ::= "throws" Type? ;
FunctionBody ::= Block ;
SimpleFunctionBody ::= Block | Statement ;

ParameterList ::= Parameter ("," Parameter)* ","? ;
Parameter ::= AttributeList? ParameterLabel? Identifier ":" Type
              ParameterModifier* DefaultArgument? ;
ParameterLabel ::= Identifier | "_" ;
ParameterModifier ::= "var" | "mut" | "inout" ;
DefaultArgument ::= "=" Expression ;
```

Sem rótulo explícito, o nome local é também o rótulo externo. `_` remove o rótulo. Argumentos posicionais devem preceder nomeados.

## 14. Lambdas e closures

A forma solta `let () => { ... }` é inválida: `let` exige binding e `=`. A forma correta é uma lambda usada como expressão.

```ebnf
LambdaExpression ::= LambdaParameterClause LambdaReturnClause?
                     LambdaEffects? "=>" LambdaBody ;
LambdaParameterClause ::= "(" LambdaParameterList? ")" ;
LambdaParameterList ::= LambdaParameter ("," LambdaParameter)* ","? ;
LambdaParameter ::= Identifier (":" Type)? ;
LambdaReturnClause ::= "->" Type ;
LambdaEffects ::= "async"? ThrowsClause? ;
LambdaBody ::= Expression | Block ;
```

```argos
let increment = (x: Int) -> Int => x + 1;

let task = () => {
    counter += 1;
};
```

Capturas são imutáveis por padrão. Captura mutável é explícita e deve respeitar ownership. Closures que escapam não podem reter referências locais inválidas.

## 15. Variáveis e padrões

```ebnf
VariableDecl ::= AttributeList? Visibility? StorageModifier?
                 BindingKind Pattern VariableType?
                 VariableInitializer? SEMI ;
BindingKind ::= "var" ("mut")? ;
StorageModifier ::= "static" | "thread_local" ;
VariableType ::= ":" Type ;
VariableInitializer ::= "=" Expression ;

Pattern ::= IdentifierPattern | WildcardPattern | TuplePattern
          | ProtocolPattern | EnumPattern | LiteralPattern ;
IdentifierPattern ::= Identifier ;
WildcardPattern ::= "_" ;
TuplePattern ::= "(" PatternList? ")" ;
PatternList ::= Pattern ("," Pattern)* ","? ;
ProtocolPattern ::= QualifiedName "(" NamedPatternList? ")" ;
NamedPatternList ::= NamedPattern ("," NamedPattern)* ","? ;
NamedPattern ::= Identifier ":" Pattern ;
EnumPattern ::= QualifiedName ("(" PatternList? ")")? ;
LiteralPattern ::= Literal ;
```

Sem tipo escrito, a declaração requer inicializador inferível. Sem inicializador, requer tipo e prova de inicialização antes do uso. `var` recebe valor uma vez; `var mut` pode ser reatribuída.

## 16. Sistema de tipos

| Tipo | Semântica | Largura |
| --- | --- | ---: |
| `bool` | lógico | 1 byte mínimo de armazenamento |
| `byte` | inteiro sem sinal | 8 bits |
| `sbyte` | inteiro com sinal | 8 bits |
| `short` / `ushort` | inteiro com/sem sinal | 16 bits |
| `int` / `uint` | inteiro com/sem sinal | 32 bits |
| `long` / `ulong` | inteiro com/sem sinal | 64 bits |
| `isize` / `usize` | inteiro da palavra-alvo | dependente do alvo |
| `float` | IEEE 754 binary32, se suportado | 32 bits |
| `double` | IEEE 754 binary64, se suportado | 64 bits |
| `char` | escalar Unicode | 32 bits lógicos |
| `void` | ausência de retorno | sem armazenamento |
| `never` | computação que não retorna | inabitável |

`Byte` é sem sinal para representar bytes, registradores e buffers. `SByte` é a contraparte assinada. `String` é tipo nominal intrínseco da biblioteca padrão, logicamente UTF-8.

```ebnf
Type ::= FunctionType OptionalSuffix*
       | PrimaryType OptionalSuffix* ;
PrimaryType ::= PrimitiveType | NamedType | TupleType | ArrayType
              | SliceType | PointerType | ReferenceType
              | InferredType | "(" Type ")" ;
PrimitiveType ::= "bool" | "byte" | "sbyte" | "short" | "ushort"
                | "int" | "uint" | "long" | "ulong"
                | "isize" | "usize" | "float" | "double"
                | "char" | "string" | "void" | "never" ;
NamedType ::= QualifiedName GenericArgumentClause? ;
TupleType ::= "(" TypeList ")" ;
TypeList ::= Type ("," Type)+ ","? ;
ArrayType ::= "[" Type ";" ConstantExpression "]" ;
SliceType ::= "[" Type "]" ;
PointerType ::= "*" ("const" | "mut")? Type ;
ReferenceType ::= "&" "mut"? Type ;
FunctionType ::= "(" FunctionTypeParameterList? ")" "->" Type ;
FunctionTypeParameterList ::= Type ("," Type)* ","? ;
OptionalSuffix ::= "?" ;
InferredType ::= "_" ;
```

Opcionais aninhados são preservados: `T??` não é automaticamente `T?`.

## 17. Null-safety

- `T` nunca contém `null`;
- `T?` contém ausência ou `T`;
- `T` promove implicitamente para `T?`;
- `T?` não converte implicitamente para `T`;
- `?.` realiza acesso seguro;
- `??` avalia fallback apenas na ausência;
- `?=` atribui somente quando a origem contém valor;
- `??=` atribui apenas quando o destino está ausente;
- `?==` compara opcionais de modo elevado;
- somente `!!` é force unwrap; `!` é exclusivamente negação lógica.

Para `?==`: duas ausências são iguais; uma ausência e um valor são diferentes; dois valores usam a igualdade do tipo-base. Operandos de short-circuit são avaliados no máximo uma vez.

## 18. Instruções

```ebnf
Statement ::= Block | VariableDecl | IfStatement | WhileStatement
            | DoWhileStatement | ForStatement | MatchStatement
            | ReturnStatement | BreakStatement | ContinueStatement
            | ThrowStatement | TryStatement | DeferStatement
            | UnsafeStatement | ExpressionStatement | EmptyStatement ;
Block ::= "{" Statement* "}" ;
SimpleBody ::= Block | Statement ;
EmptyStatement ::= SEMI ;
ExpressionStatement ::= Expression SEMI ;

IfStatement ::= "if" Expression SimpleBody ElseClause? ;
ElseClause ::= "else" (IfStatement | SimpleBody) ;
WhileStatement ::= Label? "while" Expression SimpleBody ;
DoWhileStatement ::= Label? "do" Block "while" Expression SEMI ;

ForStatement ::= Label? "for" Pattern "in" Expression Block
               | Label? "for" "(" ForInitializer? SEMI
                 Expression? SEMI ExpressionList? ")" Block ;
ForInitializer ::= VariableDeclNoTerminator | ExpressionList ;
VariableDeclNoTerminator ::= BindingKind Pattern VariableType?
                             VariableInitializer? ;

MatchStatement ::= "match" Expression "{" MatchArm* "}" ;
MatchArm ::= "case" Pattern MatchGuard? "=>" MatchBody
           | "default" "=>" MatchBody ;
MatchGuard ::= "if" Expression ;
MatchBody ::= Expression SEMI | Block ;

ReturnStatement ::= "return" Expression? SEMI ;
BreakStatement ::= "break" Identifier? SEMI ;
ContinueStatement ::= "continue" Identifier? SEMI ;
ThrowStatement ::= "throw" Expression SEMI ;
DeferStatement ::= "defer" Block ;
UnsafeStatement ::= "unsafe" Block ;
Label ::= Identifier ":" ;

TryStatement ::= "try" Block CatchClause+ FinallyClause?
               | "try" Block FinallyClause ;
CatchClause ::= "catch" ("(" Pattern (":" Type)? ")")? Block ;
FinallyClause ::= "finally" Block ;
```

Condições exigem `Bool`, não `Bool?`. Funções não `Void` retornam em todos os caminhos alcançáveis.

## 19. Expressões e precedência

| Nível | Construção | Associatividade |
| ---: | --- | --- |
| 10 | `=`, compostas, `?=`, `??=` | direita |
| 15 | lambda `=>` | direita |
| 20 | `??` | direita |
| 25 | `||` | esquerda, short-circuit |
| 30 | `&&` | esquerda, short-circuit |
| 35 | `==`, `!=`, `?==` | esquerda |
| 40 | `<`, `<=`, `>`, `>=`, `is`, `as`, `as?` | esquerda |
| 45 | `..`, `..=` | não associativo |
| 50 | `+`, `-` | esquerda |
| 60 | `*`, `/`, `%` | esquerda |
| 70 | prefixos | direita |
| 80 | chamada, índice, membro, `?.`, `!!`, `++`, `--` | esquerda |

```ebnf
Expression ::= AssignmentExpression ;
AssignmentExpression ::= LambdaExpression
                       | NullCoalescingExpression
                       | Assignable AssignmentOperator AssignmentExpression ;
AssignmentOperator ::= "=" | "+=" | "-=" | "*=" | "/=" | "%="
                     | "?=" | "??=" ;

NullCoalescingExpression ::= LogicalOrExpression
                           | LogicalOrExpression "??" NullCoalescingExpression ;
LogicalOrExpression ::= LogicalAndExpression
                      | LogicalOrExpression "||" LogicalAndExpression ;
LogicalAndExpression ::= EqualityExpression
                       | LogicalAndExpression "&&" EqualityExpression ;
EqualityExpression ::= RelationalExpression
                     | EqualityExpression EqualityOperator RelationalExpression ;
EqualityOperator ::= "==" | "!=" | "?==" ;
RelationalExpression ::= RangeExpression
                       | RelationalExpression RelationalOperator RangeExpression ;
RelationalOperator ::= "<" | "<=" | ">" | ">=" | "is" | "as" | "as?" ;
RangeExpression ::= AdditiveExpression
                  | AdditiveExpression (".." | "..=") AdditiveExpression ;
AdditiveExpression ::= MultiplicativeExpression
                     | AdditiveExpression ("+" | "-") MultiplicativeExpression ;
MultiplicativeExpression ::= PrefixExpression
                           | MultiplicativeExpression ("*" | "/" | "%") PrefixExpression ;
PrefixExpression ::= PostfixExpression
                   | PrefixOperator PrefixExpression
                   | "await" PrefixExpression | "try" PrefixExpression ;
PrefixOperator ::= "+" | "-" | "!" | "~" | "&" | "*" ;

PostfixExpression ::= PrimaryExpression
                    | PostfixExpression CallSuffix
                    | PostfixExpression IndexSuffix
                    | PostfixExpression MemberSuffix
                    | PostfixExpression GenericCallSuffix
                    | PostfixExpression PostfixOperator ;
CallSuffix ::= "(" ArgumentList? ")" ;
GenericCallSuffix ::= "::" GenericArgumentClause CallSuffix ;
IndexSuffix ::= "[" ArgumentList "]" ;
MemberSuffix ::= ("." | "?.") Identifier ;
PostfixOperator ::= "!!" | "++" | "--" ;

PrimaryExpression ::= Literal | Identifier | "this" | "super"
                    | ParenthesizedExpression | TupleExpression
                    | ArrayExpression | ObjectCreationExpression
                    | MatchExpression | ForeignExecutorExpression ;
ParenthesizedExpression ::= "(" Expression ")" ;
TupleExpression ::= "(" Expression "," ExpressionListTail? ")" ;
ExpressionListTail ::= Expression ("," Expression)* ","? ;
ArrayExpression ::= "[" ArgumentList? "]" ;
ObjectCreationExpression ::= "new" NamedType "(" ArgumentList? ")" ;

ArgumentList ::= Argument ("," Argument)* ","? ;
Argument ::= (Identifier ":")? Expression ;
ExpressionList ::= Expression ("," Expression)* ;

MatchExpression ::= "match" Expression "{" MatchExpressionArm+ "}" ;
MatchExpressionArm ::= "case" Pattern MatchGuard? "=>" Expression SEMI
                     | "default" "=>" Expression SEMI ;

Assignable ::= Identifier | PostfixExpression "." Identifier
             | PostfixExpression "[" ArgumentList "]"
             | "*" PrefixExpression ;

Literal ::= IntegerLiteral | FloatLiteral | CharLiteral | StringLiteral
          | MultilineStringLiteral | BooleanLiteral | NullLiteral ;
ConstantExpression ::= Expression ;
```

Atribuição exige l-value mutável. Operadores lógicos exigem `Bool`. Ranges são não associativos. Comparações encadeadas exigem conjunção. Ordem de avaliação é esquerda para direita, salvo short-circuit.

## 20. Herança e despacho

Classes possuem herança simples de implementação e múltipla conformidade de interfaces.

- membros são não virtuais por padrão;
- `abstract` introduz requisito;
- `override` confirma membro herdado compatível;
- `final override` impede nova sobrescrita;
- sobrecarga não se distingue apenas pelo retorno;
- campos não são sobrescritos;
- casts descendentes exigem teste ou conversão explícita.

## 21. Ownership e memória

- cada recurso possui proprietário;
- saída de escopo destrói em ordem inversa;
- movimento invalida origem não copiável;
- `&T` é referência compartilhada;
- `&mut T` é referência mutável exclusiva;
- `*const T` e `*mut T` são raw e exigem `unsafe`;
- referências não sobrevivem ao valor referenciado;
- protocols são valores e podem ser inline;
- alocações ocultas podem ser proibidas por perfil.

## 22. Unsafe e bare metal

`unsafe` autoriza desreferência raw, aritmética de ponteiros, casts de endereço, MMIO, FFI insegura e Assembly. Tipagem, escopos e mutabilidade continuam ativos.

```ebnf
UnsafeBlock ::= "unsafe" Block ;
```

`@raw` é um atributo aplicado a uma função `unsafe`; não cria uma segunda produção concorrente para funções. MMIO exige acesso volátil e barreiras apropriadas. O perfil hard real-time pode rejeitar heap, recursão, exceptions, runtimes estrangeiros e latência não limitada.

## 23. Interoperabilidade

```ebnf
ForeignDecl ::= ForeignFunctionDecl | ForeignBlockDecl ;
ForeignFunctionDecl ::= AttributeList? Visibility? "extern"
                        ForeignLanguage StringLiteral?
                        FunctionSignature SEMI ;
ForeignBlockDecl ::= "extern" ForeignLanguage "{" ForeignFunctionDecl* "}" ;
ForeignLanguage ::= "C" | "C++" | "Rust" | "Swift" | "Assembly" ;

ForeignExecutorExpression ::= ForeignExecutorName
                              "(" ForeignSource ExecutorOptionList? ")" ;
ForeignExecutorName ::= "c_executor" | "cpp_executor" | "rust_executor"
                      | "swift_executor" | "asm_executor" ;
ForeignSource ::= StringLiteral | MultilineStringLiteral ;
ExecutorOptionList ::= "," NamedArgument ("," NamedArgument)* ;
NamedArgument ::= Identifier ":" Expression ;
```

Executores não transformam Argos em linguagem estrangeira. Eles encaminham a unidade estrangeira ao frontend/toolchain apropriado e ligam ou executam o artefato resultante.

Cada executor define alvo, toolchain, modo, símbolo de entrada, ABI, dependências, stdout/stderr, timeout, isolamento, ownership e tratamento de falhas.

Uma unidade com `main` usa modo `program` isolado para não conflitar com a entrada Argos:

```argos
let result = c_executor("""
    #include <stdio.h>

    int main(void) {
        printf("Olá, Mundo!\n");
        return 0;
    }
""", mode: "program");
```

Formas equivalentes existem para C++, Rust e Swift. Assembly requer arquitetura e dialeto.

## 24. AST e HIR

A AST separa declarações, instruções, expressões, padrões, tipos, atributos e construções estrangeiras. Todo nó guarda span e identidade estável.

A HIR acrescenta símbolo, tipo, coerções, overload, bindings, capturas, categoria de valor, efeitos, região unsafe e estratégia estrangeira. AST/HIR não armazenam tipos LLVM, registradores físicos ou detalhes da VM.

## 25. Diagnósticos obrigatórios

Devem ser diagnosticados:

- nomes e tipos inexistentes;
- duplicatas e ciclos de herança/constraints;
- classe final estendida ou sealed fora do módulo;
- sobrescrita sem `override`;
- requisito abstrato ausente;
- estado armazenado em interface;
- mutação de componente de protocol;
- violações de optional e force unwrap;
- caminhos sem retorno;
- patterns não exaustivos;
- l-values inválidos;
- ponteiros fora de `unsafe`;
- executor indisponível;
- ABI incompatível;
- recurso sem lowering no backend escolhido.

Sucesso silencioso é proibido.

## 26. Exemplos integrados

### 26.1. Classe e interface

```argos
public interface Identifiable<TId> {
    public let id: TId { get }
}

public sealed class Account<TId> : Identifiable<TId>
where TId: Hashable {
    public let id: TId;
    private var balance: Long;

    public init(id: TId, initialBalance: Long = 0) {
        this.id = id;
        this.balance = initialBalance;
    }

    public final func deposit(_ amount: Long) -> Void {
        if amount > 0 {
            balance += amount;
        }
    }
}
```

### 26.2. Protocol

```argos
public protocol SensorReading<T>(
    sensor: String,
    value: T,
    timestamp: ULong
) : Serializable where T: Numeric;
```

### 26.3. Null-safety

```argos
func display(user: User?) -> String {
    return user?.profile?.name ?? "Desconhecido";
}
```

### 26.4. Bare metal

```argos
@raw
unsafe func writeRegister(address: USize, value: Byte) -> Void {
    let register: *mut Byte = address as *mut Byte;
    *register = value;
}
```

## 27. Embarcados

```text
Argos → Lexer → Parser LR(1) → AST/HIR → IR Argos
                                      ├─→ Bytecode → VM
                                      └─→ Machine IR → objeto/máquina
```

Não existe etapa C, C++ ou LLVM. Cada backend Argos conhece diretamente ISA, ABI, layout e formato do alvo.

## 28. Perfil mínimo

O primeiro perfil completo deve conter módulos simples, funções, variáveis, primitivos, `String` mínima, opcionais, classes finais, protocols, interfaces, generics monomorfizados, controle de fluxo, lambdas restritas, IR verificada, VM, um backend direto e FFI C com símbolos compilados.

Recursos posteriores falham explicitamente até possuírem semântica e lowering.

## 29. Decisões abertas

1. Classes serão finais ou abertas por padrão?
2. Haverá contagem de referências opt-in?
3. Qual será a capture list definitiva?
4. Exceptions usarão unwinding ou resultado tipado por perfil?
5. Qual será o primeiro backend nativo?
6. Bytecode será de registradores ou híbrido?
7. Quais layouts serão ABI-estáveis?
8. Executors rodam em compilação, runtime ou ambos?
9. Toolchains estrangeiras serão plugins ou componentes incorporados?

## 30. Conformidade

Uma construção é suportada somente quando possui produção LR(1) sem conflitos, CST/AST, análise semântica, HIR/IR, backend explícito e testes positivos, negativos, de recuperação e de equivalência VM/nativo.

Reconhecer sintaxe não significa implementar a linguagem.

---

## 31. Resumo da Arquitetura do Compilador para Embarcados

```text
[Código ArgosLang (.argos)]
           ↓
     [Lexer + Parser]
           ↓
   [AST + Type Checker] (Garante Imutabilidade e Null Safety)
           ↓
[Gerador de C / LLVM IR (Zero-GC)]
           ↓
[avr-gcc / xtensa-esp32-elf-gcc / clang]
           ↓
[Binário Firmware (.hex / .bin)] → Gravação na Flash do Microcontrolador (Arduino / ESP32)
```
