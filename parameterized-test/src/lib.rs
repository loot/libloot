use proc_macro::{TokenStream, TokenTree};
use quote::{ToTokens, format_ident, quote};
use syn::{Expr, ExprLit, FnArg, Ident, ItemConst, ItemFn, Lit, Pat, PatIdent, PatType, parse};

#[proc_macro_attribute]
pub fn parameterized_test(input: TokenStream, annotated_item: TokenStream) -> TokenStream {
    let macro_name: Ident = parse(input).unwrap();

    let test: ItemFn = parse(annotated_item.clone()).unwrap();

    let Some(FnArg::Typed(PatType {
        pat: type_pattern, ..
    })) = test.sig.inputs.first()
    else {
        panic!("Expected the first test function argument a type pattern");
    };

    let Pat::Ident(PatIdent {
        ident: inner_func_arg_name,
        ..
    }) = type_pattern.as_ref()
    else {
        panic!("Expected the first test function argument pattern to be an ident");
    };

    let inner_func_name = test.sig.ident.clone();

    quote! {
        mod #inner_func_name {
            use super::*;

            #test

            #macro_name!{#inner_func_name, #inner_func_arg_name}
        }
    }
    .into()
}

#[proc_macro_attribute]
pub fn test_parameter(_input: TokenStream, annotated_item: TokenStream) -> TokenStream {
    let item: ItemConst = parse(annotated_item.clone()).unwrap();

    let Expr::Array(array) = item.expr.as_ref() else {
        panic!("Expected expression to be an array");
    };

    let values: Vec<_> = array
        .elems
        .iter()
        .map(|n| match n {
            Expr::Path(path) => path.path.segments.last().unwrap().ident.to_token_stream(),
            Expr::Lit(ExprLit {
                lit: Lit::Int(lit_int),
                ..
            }) => lit_int.to_token_stream(),
            _ => panic!("Expected array element to be a path or int literal"),
        })
        .collect();

    let annotated_item = proc_macro2::TokenStream::from(annotated_item);

    let const_item_name = item.ident;
    let macro_name = format_ident!("{}_macro", &const_item_name);

    let macro_output = quote! {
        macro_rules! #macro_name {
            ( $inner_test_name:ident, $inner_test_arg_name:ident ) => {
                parameterized_test::generate_tests!{
                    $inner_test_name,
                    $inner_test_arg_name,
                    #const_item_name,
                    [#(#values),*]
                }
            };
        }

        #[allow(unused_imports)]
        pub(crate) use #macro_name as #const_item_name;

        #annotated_item
    };

    macro_output.into()
}

#[proc_macro]
pub fn generate_tests(item: TokenStream) -> TokenStream {
    let mut item_iter = item.into_iter();
    let TokenTree::Ident(inner_test_name) = item_iter.next().unwrap() else {
        panic!("Expected an ident for the inner_test_name");
    };

    let _ = item_iter.next();

    let TokenTree::Ident(inner_test_param_name) = item_iter.next().unwrap() else {
        panic!("Expected an ident for the inner_test_param_name");
    };

    let _ = item_iter.next();

    let TokenTree::Ident(const_item_name) = item_iter.next().unwrap() else {
        panic!("Expected an ident for the const_item_name");
    };

    let _ = item_iter.next();

    let TokenTree::Group(const_item_values) = item_iter.next().unwrap() else {
        panic!("Expected a group for the const_item_values");
    };

    let inner_test_name: Ident = parse(TokenTree::from(inner_test_name).into()).unwrap();
    let const_item_name: Ident = parse(TokenTree::from(const_item_name).into()).unwrap();

    let tokens: proc_macro2::TokenStream = const_item_values
        .stream()
        .into_iter()
        .step_by(2)
        .enumerate()
        .flat_map(|(i, value)| {
            let suffix = match value {
                TokenTree::Ident(ident) => ident.to_string(),
                TokenTree::Literal(literal) => literal.to_string(),
                _ => panic!("Expected const item value to be an ident or literal"),
            };

            let test_name = format_ident!("{inner_test_param_name}_{i:02}_{suffix}");

            quote! {
                #[test]
                #[allow(non_snake_case)]
                fn #test_name() {
                    #inner_test_name(#const_item_name[#i]);
                }
            }
        })
        .collect();

    tokens.into()
}
