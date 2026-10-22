#pragma once

#include "argos/diagnostics/diagnostic_definition.hpp"

namespace argos::diagnostics::catalog {

namespace lexer {
inline constexpr DiagnosticDefinition unterminated_block_comment{
    "ARGOS-L0001", Severity::error, "comentário de bloco não terminado",
    "Um comentário iniciado por /* chegou ao fim do arquivo sem */.",
    "Feche o comentário com */."};
inline constexpr DiagnosticDefinition invalid_token{
    "ARGOS-L0002", Severity::error, "token inválido",
    "O caractere ou literal não forma um token válido na ArgosLang.",
    "Corrija ou remova o trecho indicado."};
inline constexpr DiagnosticDefinition invalid_utf8{
    "ARGOS-L0003", Severity::error, "sequência UTF-8 inválida",
    "O arquivo-fonte contém bytes que não representam texto UTF-8 válido.",
    "Salve o arquivo como UTF-8."};
} // namespace lexer

namespace parser {
inline constexpr DiagnosticDefinition unexpected_syntax{
    "ARGOS-P0001", Severity::error, "sintaxe inesperada",
    "A sequência de tokens não corresponde à gramática esperada neste ponto.",
    "Leia a mensagem detalhada e corrija o token indicado."};
inline constexpr DiagnosticDefinition malformed_identifier{
    "ARGOS-P0002", Severity::error, "identificador malformado",
    "Nomes devem começar por letra, _ ou caractere Unicode identificador válido.",
    "Use, por exemplo, total, _index ou nomeUsuario."};
inline constexpr DiagnosticDefinition reserved_identifier{
    "ARGOS-P0003", Severity::error, "palavra reservada usada como identificador",
    "Palavras-chave da linguagem não podem nomear variáveis, funções ou parâmetros.",
    "Escolha outro nome ou use um identificador escapado entre crases."};
inline constexpr DiagnosticDefinition incomplete_keyword{
    "ARGOS-P0004", Severity::error, "palavra-chave incompleta ou desconhecida",
    "Uma palavra parecida com uma construção da linguagem não foi reconhecida.",
    "Use a grafia canônica, por exemplo function main() -> void { ... }."};
} // namespace parser

namespace compiler {
inline constexpr DiagnosticDefinition unsupported_lowering{
    "ARGOS-C0001", Severity::error, "construção ainda não é compilável",
    "O frontend aceitou a construção, mas a IR/VM/backend nativo ainda não a suporta.",
    "Use o REPL ou o subconjunto executável documentado."};
inline constexpr DiagnosticDefinition invalid_entry_point{
    "ARGOS-C0002", Severity::error, "ponto de entrada inválido",
    "Um executável ArgosLang precisa de uma function main sem parâmetros.",
    "Declare function main() -> void ou function main() -> int."};
} // namespace compiler

namespace warning {
inline constexpr DiagnosticDefinition legacy_function_keyword{
    "ARGOS-W0001", Severity::warning, "grafia legada de function",
    "'func' continua aceito apenas para facilitar a migração de fontes antigas.",
    "Use 'function' em código novo."};
inline constexpr DiagnosticDefinition legacy_null_keyword{
    "ARGOS-W0002", Severity::warning, "grafia legada de nil",
    "'null' continua aceito apenas para facilitar a migração de fontes antigas.",
    "Use 'nil' em código novo."};
} // namespace warning

namespace semantic {
constexpr DiagnosticDefinition semantic_error(std::string_view code,
                                              std::string_view title,
                                              std::string_view help) {
  return {code, Severity::error, title,
          "A análise semântica encontrou uma regra da linguagem violada.", help};
}
namespace symbols {
inline constexpr DiagnosticDefinition duplicate_declaration{"ARGOS-S0001", Severity::error, "declaração duplicada", "Um nome foi declarado mais de uma vez no mesmo escopo.", "Renomeie uma declaração ou remova a duplicada."};
inline constexpr DiagnosticDefinition invalid_ast_node{"ARGOS-S0002", Severity::error, "nó de AST inválido", "A construção não é aceita naquela posição semântica.", "Revise a estrutura da declaração ou bloco."};
inline constexpr DiagnosticDefinition unknown_declaration{"ARGOS-S0003", Severity::error, "declaração semântica desconhecida", "O analisador não reconhece essa declaração na fase atual.", "Use uma declaração suportada ou atualize o frontend."};
inline constexpr DiagnosticDefinition unknown_statement{"ARGOS-S0004", Severity::error, "instrução semântica desconhecida", "O analisador não reconhece essa instrução na fase atual.", "Use uma instrução suportada ou atualize o frontend."};
inline constexpr DiagnosticDefinition unknown_expression{"ARGOS-S0005", Severity::error, "expressão semântica desconhecida", "O analisador não reconhece essa expressão na fase atual.", "Use uma expressão suportada ou atualize o frontend."};
inline constexpr DiagnosticDefinition unknown_identifier{"ARGOS-S0300", Severity::error, "identificador não declarado", "O nome não está visível neste escopo.", "Declare-o antes de usar ou corrija a grafia."};
inline constexpr DiagnosticDefinition uninitialized_variable{"ARGOS-S0301", Severity::error, "variável usada antes da inicialização", "A variável não recebeu valor antes da leitura.", "Inicialize a variável antes de usá-la."};
inline constexpr DiagnosticDefinition type_used_as_value{"ARGOS-S0302", Severity::error, "tipo usado como valor", "Um nome de tipo não pode ser avaliado como expressão.", "Use uma variável ou uma construção explícita."};
} // namespace symbols
namespace declarations {
inline constexpr DiagnosticDefinition invalid_variable_type{"ARGOS-S0100", Severity::error, "tipo inválido para variável", "void e never não podem ser tipos de armazenamento.", "Use um tipo que represente valores."};
inline constexpr DiagnosticDefinition incompatible_initializer{"ARGOS-S0101", Severity::error, "inicializador incompatível", "O valor inicial não pode ser atribuído ao tipo declarado.", "Ajuste o tipo ou a expressão inicializadora."};
inline constexpr DiagnosticDefinition missing_variable_type_or_value{"ARGOS-S0103", Severity::error, "variável sem tipo e sem valor", "Não há informação suficiente para validar a declaração.", "Informe tipo e inicializador."};
inline constexpr DiagnosticDefinition uninferable_variable_type{"ARGOS-S0102", Severity::error, "tipo de variável não inferível", "O inicializador não permite deduzir um tipo seguro.", "Declare o tipo explicitamente."};
inline constexpr DiagnosticDefinition integer_literal_out_of_inferred_range{"ARGOS-S0104", Severity::error, "literal inteiro fora do intervalo inferido", "O literal não cabe em int, tipo padrão de inferência inteira.", "Declare um tipo inteiro maior ou ajuste o literal."};
} // namespace declarations
namespace operators {
inline constexpr auto unary_numeric = semantic_error("ARGOS-S0310", "operador unário aritmético exige número", "Use int, double ou outro tipo numérico não opcional.");
inline constexpr auto logical_not = semantic_error("ARGOS-S0311", "negação lógica exige bool", "Use ! apenas sobre uma expressão bool.");
inline constexpr auto arithmetic_operands = semantic_error("ARGOS-S0320", "operação aritmética exige números", "Use operandos numéricos não opcionais.");
inline constexpr auto arithmetic_type_mismatch = semantic_error("ARGOS-S0321", "operandos aritméticos incompatíveis", "Use tipos numéricos compatíveis.");
inline constexpr auto equality_type_mismatch = semantic_error("ARGOS-S0322", "operandos de igualdade incompatíveis", "Compare valores de tipos compatíveis.");
inline constexpr auto lifted_equality_requires_optional = semantic_error("ARGOS-S0323", "igualdade segura exige opcional ou nil", "Use ?== somente com opcionais ou nil.");
inline constexpr auto coalesce_requires_optional = semantic_error("ARGOS-S0324", "coalescência exige opcional", "Use ?? em um valor T?.");
inline constexpr auto coalesce_fallback_mismatch = semantic_error("ARGOS-S0325", "valor padrão incompatível", "O lado direito de ?? deve ser compatível com T.");
inline constexpr auto relational_operands = semantic_error("ARGOS-S0326", "comparação relacional exige números", "Compare valores numéricos não opcionais.");
inline constexpr auto logical_operands = semantic_error("ARGOS-S0327", "operador lógico exige bool", "Use && e || apenas com bool.");
inline constexpr auto relational_type_mismatch = semantic_error("ARGOS-S0328", "comparação relacional incompatível", "Use tipos numéricos compatíveis.");
inline constexpr auto lifted_equality_type_mismatch = semantic_error("ARGOS-S0329", "igualdade segura incompatível", "Compare opcionais com tipos-base compatíveis.");
} // namespace operators
namespace assignment {
inline constexpr auto invalid_target = semantic_error("ARGOS-S0330", "alvo de atribuição inválido", "Atribua apenas a uma variável ou membro reatribuível.");
inline constexpr auto immutable_target = semantic_error("ARGOS-S0331", "atribuição a variável imutável", "Declare a variável com var mut para reatribuí-la.");
inline constexpr auto incompatible_value = semantic_error("ARGOS-S0332", "valor atribuído incompatível", "Atribua um valor compatível com o tipo do alvo.");
inline constexpr auto compound_requires_numeric = semantic_error("ARGOS-S0333", "atribuição composta exige número", "Use +=, -=, *=, /= ou %= com números.");
inline constexpr auto compound_type_mismatch = semantic_error("ARGOS-S0334", "atribuição composta incompatível", "Use valores numéricos compatíveis.");
inline constexpr auto coalescing_value_mismatch = semantic_error("ARGOS-S0335", "valor de ?" "?= incompatível", "Atribua um valor compatível com o tipo desembrulhado.");
inline constexpr auto safe_assignment_requires_optional = semantic_error("ARGOS-S0336", "?= exige alvo opcional", "Use ?= apenas em uma variável T?.");
inline constexpr auto unknown_assignment_target = semantic_error("ARGOS-S0337", "alvo de atribuição não declarado", "Declare a variável antes de atribuir.");
} // namespace assignment
namespace control {
inline constexpr DiagnosticDefinition return_outside_function{"ARGOS-S0200", Severity::error, "return fora de função", "return só pode encerrar uma function.", "Mova-o para o corpo de uma função."};
inline constexpr DiagnosticDefinition incompatible_return{"ARGOS-S0201", Severity::error, "retorno incompatível", "O valor retornado não corresponde ao tipo declarado.", "Retorne um valor compatível ou altere a assinatura."};
inline constexpr DiagnosticDefinition missing_return{"ARGOS-S0202", Severity::error, "retorno obrigatório ausente", "Uma função com retorno não-void pode terminar sem retornar.", "Retorne um valor em todos os caminhos."};
inline constexpr DiagnosticDefinition break_outside_loop{"ARGOS-S0203", Severity::error, "break fora de laço", "break só pode ser usado dentro de while ou futuro for.", "Remova-o ou coloque-o dentro de um laço."};
inline constexpr DiagnosticDefinition continue_outside_loop{"ARGOS-S0204", Severity::error, "continue fora de laço", "continue só pode ser usado dentro de while ou futuro for.", "Remova-o ou coloque-o dentro de um laço."};
} // namespace control
namespace types {
inline constexpr DiagnosticDefinition unknown_type{"ARGOS-S0400", Severity::error, "tipo não declarado", "O nome de tipo não foi encontrado.", "Use um tipo primitivo ou declare o tipo antes do uso."};
inline constexpr DiagnosticDefinition condition_not_boolean{"ARGOS-S0370", Severity::error, "condição não booleana", "if e while exigem uma expressão bool.", "Use uma comparação ou expressão booleana."};
inline constexpr auto conditional_branch_mismatch = semantic_error("ARGOS-S0380", "ramos condicionais incompatíveis", "Faça os dois ramos produzirem tipos compatíveis.");
} // namespace types
namespace members {
inline constexpr auto optional_member_requires_safe_access = semantic_error("ARGOS-S0350", "membro de opcional exige acesso seguro", "Use ?. ou desembrulhe o valor antes do acesso.");
inline constexpr auto member_base_not_nominal = semantic_error("ARGOS-S0351", "acesso de membro em tipo não nominal", "Acesse membros apenas de valores que os declaram.");
inline constexpr auto unknown_member = semantic_error("ARGOS-S0352", "membro não declarado", "Corrija o nome do membro ou declare-o no tipo.");
inline constexpr auto unsafe_access_on_optional = semantic_error("ARGOS-S0353", "acesso inseguro em opcional", "Use ?. para acesso condicional.");
} // namespace members
namespace postfix {
inline constexpr auto increment_requires_mutable_numeric = semantic_error("ARGOS-S0360", "incremento exige variável numérica mutável", "Use ++ apenas em var mut numérica.");
inline constexpr auto decrement_requires_mutable_numeric = semantic_error("ARGOS-S0361", "decremento exige variável numérica mutável", "Use -- apenas em var mut numérica.");
inline constexpr auto unwrap_requires_optional = semantic_error("ARGOS-S0362", "desembrulhamento exige opcional", "Use !! apenas em um valor T?.");
inline constexpr auto unsafe_unwrap = semantic_error("ARGOS-S0363", "desembrulhamento inseguro", "Verifique nil antes de usar !!.");
inline constexpr auto unsupported_propagation = semantic_error("ARGOS-S0364", "propagação pós-fixa indisponível", "Esse operador ainda não pertence à gramática atual.");
inline constexpr auto invalid_postfix_not = semantic_error("ARGOS-S0365", "! pós-fixo inválido", "Use ! para negação ou !! para desembrulhar opcional.");
} // namespace postfix
namespace protocols {
inline constexpr auto duplicate_protocol_component = semantic_error("ARGOS-S0500", "componente de protocolo duplicado", "Cada componente precisa ter nome único.");
inline constexpr auto invalid_protocol_component_type = semantic_error("ARGOS-S0501", "tipo de componente inválido", "Use um tipo de armazenamento válido.");
inline constexpr auto incompatible_protocol_default = semantic_error("ARGOS-S0502", "valor padrão de protocolo incompatível", "Use um valor compatível com o tipo do componente.");
} // namespace protocols
} // namespace semantic

namespace semantic::calls {
inline constexpr DiagnosticDefinition non_named_callee{
    "ARGOS-S0340", Severity::error, "alvo de chamada não é uma função nomeada",
    "Esta versão só resolve chamadas cujo alvo é um identificador de função.",
    "Declare uma function com nome e chame-a diretamente."};
inline constexpr DiagnosticDefinition undeclared_function{
    "ARGOS-S0341", Severity::error, "função não declarada",
    "O identificador usado na chamada não resolve para uma função visível.",
    "Declare a função antes de chamá-la ou corrija o nome."};
inline constexpr DiagnosticDefinition missing_argument{
    "ARGOS-S0342", Severity::error, "argumento obrigatório ausente",
    "Um parâmetro da função não recebeu argumento.",
    "Forneça o argumento exigido pela assinatura."};
inline constexpr DiagnosticDefinition incompatible_argument{
    "ARGOS-S0343", Severity::error, "tipo de argumento incompatível",
    "O valor passado não pode ser atribuído ao parâmetro correspondente.",
    "Passe um valor do tipo exigido pelo parâmetro."};
inline constexpr DiagnosticDefinition positional_argument_without_slot{
    "ARGOS-S0344", Severity::error, "argumento posicional sem parâmetro posicional",
    "A assinatura espera um rótulo neste ponto da chamada.",
    "Use o rótulo do parâmetro ou ajuste a ordem dos argumentos."};
inline constexpr DiagnosticDefinition unknown_argument_label{
    "ARGOS-S0345", Severity::error, "rótulo de argumento inexistente",
    "Nenhum parâmetro da função possui esse rótulo externo.",
    "Use um rótulo declarado na assinatura da função."};
inline constexpr DiagnosticDefinition duplicate_argument{
    "ARGOS-S0346", Severity::error, "parâmetro recebeu mais de um argumento",
    "O mesmo parâmetro foi preenchido por argumentos diferentes.",
    "Remova o argumento repetido ou corrija os rótulos."};
inline constexpr DiagnosticDefinition duplicate_parameter_label{
    "ARGOS-S0347", Severity::error, "rótulo de parâmetro duplicado",
    "Dois parâmetros da mesma função possuem o mesmo rótulo externo.",
    "Dê a cada parâmetro rotulado um nome externo único."};
} // namespace semantic::calls

} // namespace argos::diagnostics::catalog
